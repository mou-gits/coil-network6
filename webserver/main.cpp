#include "crow_lib/include/crow.h"
#include "../PktDefLib/PktDef.h"
#include "../mysocketlib/MySocket.h"
#include <mutex>
#include <vector>
#include <sstream>
#include <ctime>
#include <cctype>
#include <fstream>
#include <cstring>
#include <array>

// Global state
static MySocket* robotSocket = nullptr;
static std::mutex socketMtx;
static int pktCounter = 0;
static std::vector<std::string> packetLog;
static std::mutex logMtx;

static std::string robotIP;
static int robotPort = 0;

struct RoutingConfig {
    bool enabled = false;
    std::string targetIp;
    int targetPort = 0;
};

static RoutingConfig routingConfig;
static std::mutex routingMtx;

static void Log(const std::string& entry)
{
    std::lock_guard<std::mutex> lk(logMtx);
    time_t now = time(nullptr);
    char ts[64];
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    strftime(ts, sizeof(ts), "%H:%M:%S", &timeinfo);
    packetLog.push_back(std::string(ts) + " " + entry);
    if (packetLog.size() > 200) packetLog.erase(packetLog.begin());
}

static crow::json::wvalue ForwardToInstance(const std::string& method,
                                            const std::string& path,
                                            const std::string& body = "")
{
    crow::json::wvalue res;

    RoutingConfig cfg;
    {
        std::lock_guard<std::mutex> lk(routingMtx);
        cfg = routingConfig;
    }

    if (!cfg.enabled || cfg.targetIp.empty() || cfg.targetPort <= 0) {
        res["error"] = "Routing target is not configured";
        return res;
    }

    try {
        MySocket httpClient(CLIENT, cfg.targetIp, static_cast<unsigned int>(cfg.targetPort), TCP, 8192);
        httpClient.ConnectTCP();

        std::string request = method + " " + path + " HTTP/1.1\r\n";
        request += "Host: " + cfg.targetIp + ":" + std::to_string(cfg.targetPort) + "\r\n";
        request += "Connection: close\r\n";
        if (!body.empty()) {
            request += "Content-Type: application/json\r\n";
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        } else {
            request += "Content-Length: 0\r\n";
        }
        request += "\r\n";
        request += body;

        httpClient.SendData(request.c_str(), static_cast<int>(request.size()));
        Log("ROUTE TX " + method + " " + path + " -> " + cfg.targetIp + ":" + std::to_string(cfg.targetPort));

        std::string response;
        std::array<char, 8192> chunk{};

        while (true) {
            try {
                int got = httpClient.GetData(chunk.data());
                if (got <= 0) break;
                response.append(chunk.data(), got);
                if (got < static_cast<int>(chunk.size())) break;
            } catch (const std::exception& e) {
                // Timeout after receiving partial HTTP response is acceptable.
                if (response.empty()) {
                    throw;
                }
                std::string msg = e.what();
                if (msg.find("timed out") == std::string::npos) {
                    throw;
                }
                break;
            }
        }

        auto bodyPos = response.find("\r\n\r\n");
        if (bodyPos == std::string::npos) {
            res["error"] = "Invalid HTTP response from routed instance";
            return res;
        }

        std::string responseBody = response.substr(bodyPos + 4);
        auto forwarded = crow::json::load(responseBody);
        if (!forwarded) {
            res["error"] = "Routed instance returned non-JSON response";
            res["raw"] = responseBody;
            return res;
        }

        Log("ROUTE RX " + method + " " + path + " <- " + cfg.targetIp + ":" + std::to_string(cfg.targetPort));
        return forwarded;
    } catch (const std::exception& e) {
        res["error"] = std::string("Routing failed: ") + e.what();
        Log("ROUTE ERROR: " + std::string(e.what()));
        return res;
    }
}

// Send a packet and get the response, returns JSON-friendly info
static crow::json::wvalue SendAndRecv(PktDef& pkt)
{
    crow::json::wvalue res;
    std::lock_guard<std::mutex> lk(socketMtx);

    if (!robotSocket) {
        res["error"] = "Not connected";
        return res;
    }

    pkt.SetPktCount(++pktCounter);
    const int txPktCount = pktCounter;
    char* raw = pkt.GenPacket();
    int len = pkt.GetLength();

    Log("TX pkt#" + std::to_string(pktCounter) + " len=" + std::to_string(len) +
        " flags=0x" + std::to_string(pkt.GetFlags()));

    try {
        robotSocket->SendData(raw, len);
    } catch (std::exception& e) {
        res["error"] = std::string("Send failed: ") + e.what();
        Log("TX ERROR: " + std::string(e.what()));
        return res;
    }

    char recvBuf[1024] = {};
    int bytes = 0;
    try {
        bytes = robotSocket->GetData(recvBuf);
    } catch (std::exception& e) {
        res["error"] = std::string("Recv failed: ") + e.what();
        Log("RX ERROR: " + std::string(e.what()));
        return res;
    }

    if (bytes <= 0) {
        res["error"] = "No response";
        Log("RX: no data");
        return res;
    }

    PktDef resp(recvBuf);
    res["ack"] = resp.GetAck();
    // Keep pktCount as the webserver transmit counter for clarity.
    // Robot response counter is returned separately as rxPktCount.
    res["pktCount"] = txPktCount;
    res["txPktCount"] = txPktCount;
    res["rxPktCount"] = resp.GetPktCount();
    res["length"] = resp.GetLength();
    res["flags"] = resp.GetFlags();

    // Only attach telemetry when response body actually looks like telemetry.
    // This avoids overwriting UI with zeros from non-telemetry ACK messages.
    const bool hasStatusFlag = (resp.GetFlags() & 0x02) != 0;
    const bool hasTelemetryBody =
        hasStatusFlag && resp.GetBodySize() >= static_cast<int>(sizeof(TelemetryBody));
    if (hasTelemetryBody) {
        TelemetryBody t{};
        char* rawResp = resp.GetRawBuffer();
        if (rawResp != nullptr) {
            std::memcpy(&t, rawResp + HEADERSIZE, sizeof(TelemetryBody));
        } else {
            t = resp.GetTelemetry();
        }

        res["telemetry"]["lastPktCounter"] = t.LastPktCounter;
        res["telemetry"]["currentGrade"] = t.CurrentGrade;
        res["telemetry"]["hitCount"] = t.HitCount;
        res["telemetry"]["heading"] = t.Heading;
        res["telemetry"]["lastCmd"] = t.LastCmd;
        res["telemetry"]["lastCmdValue"] = t.LastCmdValue;
        res["telemetry"]["lastCmdPower"] = t.LastCmdPower;
    }

    // Also include message if present
    int msgSize = 0;
    char* msg = resp.GetBodyData(msgSize);
    if (msg && msgSize > 0)
        res["message"] = std::string(msg, msgSize);

    Log("RX " + std::string(resp.GetAck() ? "ACK" : "NACK") +
        " pkt#" + std::to_string(resp.GetPktCount()));

    return res;
}

int main()
{
    crow::SimpleApp app;

    // ============================================================
    // ROOT ROUTE: / - Serve Command and Control GUI
    // ============================================================
    CROW_ROUTE(app, "/")
    ([]() {
        // Try common run locations (solution root, webserver output dirs, etc.)
        const char* candidates[] = {
            "webserver/static/index.html",
            "static/index.html",
            "../webserver/static/index.html",
            "../static/index.html",
            "../../webserver/static/index.html"
        };

        std::ifstream file;
        for (const char* path : candidates) {
            file.open(path, std::ios::in);
            if (file.is_open()) {
                break;
            }
            file.clear();
        }

        if (!file.is_open()) {
            return crow::response(404, "File not found");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        crow::response r(buffer.str());
        r.set_header("Content-Type", "text/html; charset=utf-8");
        r.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        r.set_header("Pragma", "no-cache");
        r.set_header("Expires", "0");
        return r;
    });

    // ============================================================
    // CONNECTION ROUTE: /connect/<string>/<int> - Connect to Robot
    // Parameters: <string> = IP Address, <int> = Port Number
    // Method: POST
    // ============================================================
    CROW_ROUTE(app, "/connect/<string>/<int>").methods(crow::HTTPMethod::Post)
    ([](const std::string& ip, int port) {
        std::lock_guard<std::mutex> lk(socketMtx);

        // Validate IP address format
        if (ip.empty()) {
            Log("Connection failed: Empty IP address");
            return crow::json::wvalue({{"error", "IP address cannot be empty"}});
        }

        // Validate port number range
        if (port <= 0 || port > 65535) {
            Log("Connection failed: Invalid port " + std::to_string(port));
            return crow::json::wvalue({{"error", "Port must be between 1 and 65535"}});
        }

        // Basic IP validation - check for basic format
        int dotCount = 0;
        bool hasValidOctets = true;
        std::string octets[4];
        int octetIndex = 0;

        for (size_t i = 0; i < ip.length(); i++) {
            if (ip[i] == '.') {
                dotCount++;
                octetIndex++;
            } else if (!isdigit(ip[i])) {
                hasValidOctets = false;
                break;
            } else {
                if (octetIndex < 4) {
                    octets[octetIndex] += ip[i];
                }
            }
        }

        // For IPv4: must have exactly 3 dots (4 octets)
        // octetIndex should equal 3 (since we start at 0 and increment when we see dots)
        if (dotCount != 3 || !hasValidOctets || octetIndex != 3) {
            // Also allow "localhost" as special case
            if (ip != "localhost" && ip != "127.0.0.1") {
                Log("Connection failed: Invalid IP format: " + ip);
                return crow::json::wvalue({{"error", "Invalid IP address format"}});
            }
        }

        // Clean up old connection if exists
        if (robotSocket) {
            delete robotSocket;
            robotSocket = nullptr;
        }

        ConnectionType connectionType = UDP;
        if (port == 29000) {
            connectionType = TCP;
        } else if (port == 29500) {
            connectionType = UDP;
        } else {
            return crow::json::wvalue({{"error", "Port must be 29000 (TCP) or 29500 (UDP)"}});
        }

        try {
            robotIP = ip;
            robotPort = port;
            robotSocket = new MySocket(CLIENT, ip, port, connectionType, 1024);
            if (connectionType == TCP) {
                robotSocket->ConnectTCP();
            }
            pktCounter = 0;
            Log("Connected to " + ip + ":" + std::to_string(port) + " via " +
                (connectionType == TCP ? "TCP" : "UDP"));
            return crow::json::wvalue({{"status", "connected"},
                                        {"ip", ip}, {"port", port},
                                        {"protocol", connectionType == TCP ? "tcp" : "udp"}});
        } catch (std::exception& e) {
            Log("Connect failed: " + std::string(e.what()));
            robotSocket = nullptr;
            return crow::json::wvalue({{"error", "Connection failed: " + std::string(e.what())}});
        }
    });

    // ============================================================
    // TELECOMMAND ROUTE: /telecommand/ - Send Drive or Sleep Command
    // Method: PUT
    // Body: JSON with command details (drive/sleep)
    // ============================================================
    CROW_ROUTE(app, "/telecommand/").methods(crow::HTTPMethod::Put)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::json::wvalue({{"error", "Invalid JSON"}});

        std::string cmd = body["command"].s();
        PktDef pkt;

        if (cmd == "sleep") {
            pkt.SetCmd(SLEEP);
            Log("Command: SLEEP");
        } else if (cmd == "drive") {
            int dir = body["direction"].i();
            int duration = body["duration"].i();

            if (duration < 0) {
                return crow::json::wvalue({{"error", "Duration must be non-negative"}});
            }

            if (dir == LEFT || dir == RIGHT) {
                if (duration > 65535) {
                    return crow::json::wvalue({{"error", "Turn duration must be 0-65535"}});
                }
                TurnBody t{};
                t.Direction = static_cast<unsigned char>(dir);
                t.Duration = static_cast<unsigned short>(duration);
                pkt.SetTurnBody(t);
                Log("Command: DRIVE " + std::string(dir == LEFT ? "LEFT" : "RIGHT") + 
                    " duration=" + std::to_string(duration));
            } else if (dir == FORWARD || dir == BACKWARD) {
                int power = body["power"].i();
                if (duration > 255) {
                    return crow::json::wvalue({{"error", "Drive duration must be 0-255"}});
                }
                if (power < 80 || power > 100) {
                    return crow::json::wvalue({{"error", "Power must be 80-100"}});
                }
                DriveBody d{};
                d.Direction = static_cast<unsigned char>(dir);
                d.Duration = static_cast<unsigned char>(duration);
                d.Power = static_cast<unsigned char>(power);
                pkt.SetDriveBody(d);
                Log("Command: DRIVE " + std::string(dir == FORWARD ? "FORWARD" : "BACKWARD") + 
                    " duration=" + std::to_string(duration) + " power=" + std::to_string(power));
            } else {
                return crow::json::wvalue({{"error", "Invalid direction"}});
            }
        } else {
            return crow::json::wvalue({{"error", "Unknown command"}});
        }

        RoutingConfig cfg;
        {
            std::lock_guard<std::mutex> lk(routingMtx);
            cfg = routingConfig;
        }
        if (cfg.enabled) {
            return ForwardToInstance("PUT", "/telecommand/", req.body);
        }

        return SendAndRecv(pkt);
    });

    // ============================================================
    // TELEMETRY REQUEST ROUTE: /telementry_request/ - Get Robot Telemetry
    // NOTE: Endpoint name has typo "telementry" per specification
    // Method: GET
    // ============================================================
    CROW_ROUTE(app, "/telementry_request/").methods(crow::HTTPMethod::Get)
    ([]() {
        RoutingConfig cfg;
        {
            std::lock_guard<std::mutex> lk(routingMtx);
            cfg = routingConfig;
        }
        if (cfg.enabled) {
            return ForwardToInstance("GET", "/telementry_request/");
        }

        PktDef pkt;
        pkt.SetCmd(RESPONSE);
        Log("Telemetry Request");
        return SendAndRecv(pkt);
    });

    // ============================================================
    // ROUTING TABLE ROUTE: /routing_table/ - Route Commands/Telemetry
    // Method: POST
    // ============================================================
    CROW_ROUTE(app, "/routing_table/").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::json::wvalue({{"error", "Invalid JSON"}});

        std::string mode = "direct";
        if (body.has("mode") && body["mode"].t() == crow::json::type::String) {
            mode = body["mode"].s();
            for (char& ch : mode) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (mode != "direct" && mode != "route") {
            return crow::json::wvalue({{"error", "mode must be direct or route"}});
        }

        std::lock_guard<std::mutex> lk(routingMtx);
        if (mode == "direct") {
            routingConfig.enabled = false;
            routingConfig.targetIp.clear();
            routingConfig.targetPort = 0;
            Log("Routing disabled (direct robot mode)");
            return crow::json::wvalue({{"status", "routing_disabled"}, {"mode", "direct"}});
        }

        if (!body.has("targetIp") || !body.has("targetPort")) {
            return crow::json::wvalue({{"error", "route mode requires targetIp and targetPort"}});
        }

        std::string targetIp = body["targetIp"].s();
        int targetPort = body["targetPort"].i();
        if (targetIp.empty() || targetPort <= 0 || targetPort > 65535) {
            return crow::json::wvalue({{"error", "invalid route target"}});
        }

        routingConfig.enabled = true;
        routingConfig.targetIp = targetIp;
        routingConfig.targetPort = targetPort;
        Log("Routing enabled -> " + targetIp + ":" + std::to_string(targetPort));
        return crow::json::wvalue({{"status", "routing_enabled"},
                                    {"mode", "route"},
                                    {"targetIp", targetIp},
                                    {"targetPort", targetPort}});
    });

    // ============================================================
    // PACKET LOG ROUTE: /log - Get Communication Log
    // Method: GET
    // ============================================================
    CROW_ROUTE(app, "/log").methods(crow::HTTPMethod::Get)
    ([]() {
        std::lock_guard<std::mutex> lk(logMtx);
        crow::json::wvalue res;
        for (size_t i = 0; i < packetLog.size(); ++i) {
            res["log"][i] = packetLog[i];
        }
        return res;
    });

    std::cout << "Starting Command & Control GUI on http://0.0.0.0:8080" << std::endl;
    std::cout << "\nAvailable Routes:" << std::endl;
    std::cout << "  GET    / - Serve Command and Control GUI" << std::endl;
    std::cout << "  POST   /connect/<ip>/<port> - Connect to robot" << std::endl;
    std::cout << "  PUT    /telecommand/ - Send drive/sleep command" << std::endl;
    std::cout << "  GET    /telementry_request/ - Request telemetry" << std::endl;
    std::cout << "  POST   /routing_table/ - Route commands to different instance" << std::endl;
    std::cout << "  GET    /log - Get packet communication log" << std::endl;

    app.port(8080).multithreaded().run();

    delete robotSocket;
    return 0;
}
