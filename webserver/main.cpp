#include "crow_lib/include/crow.h"
#include "../PktDefLib/PktDef.h"
#include "../mysocketlib/MySocket.h"
#include <mutex>
#include <vector>
#include <sstream>
#include <ctime>

// Global state
static MySocket* robotSocket = nullptr;
static std::mutex socketMtx;
static int pktCounter = 0;
static std::vector<std::string> packetLog;
static std::mutex logMtx;

static std::string robotIP;
static int robotPort = 0;

static void Log(const std::string& entry)
{
    std::lock_guard<std::mutex> lk(logMtx);
    time_t now = time(nullptr);
    char ts[64];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));
    packetLog.push_back(std::string(ts) + " " + entry);
    if (packetLog.size() > 200) packetLog.erase(packetLog.begin());
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
    res["pktCount"] = resp.GetPktCount();
    res["length"] = resp.GetLength();
    res["flags"] = resp.GetFlags();

    if (resp.IsTelemetryResponse()) {
        TelemetryBody t = resp.GetTelemetry();
        res["telemetry"]["lastPktCounter"] = t.LastPktCounter;
        res["telemetry"]["currentGrade"] = t.CurrentGrade;
        res["telemetry"]["hitCount"] = t.HitCount;
        res["telemetry"]["heading"] = t.Heading;
        res["telemetry"]["lastCmd"] = t.LastCmd;
        res["telemetry"]["lastCmdValue"] = t.LastCmdValue;
        res["telemetry"]["lastCmdPower"] = t.LastCmdPower;
        Log("RX Telemetry: grade=" + std::to_string(t.CurrentGrade) +
            " hits=" + std::to_string(t.HitCount) +
            " heading=" + std::to_string(t.Heading));
    } else {
        int msgSize = 0;
        char* msg = resp.GetBodyData(msgSize);
        if (msg && msgSize > 0)
            res["message"] = std::string(msg, msgSize);

        Log("RX " + std::string(resp.GetAck() ? "ACK" : "NACK") +
            " pkt#" + std::to_string(resp.GetPktCount()));
    }

    return res;
}

int main()
{
    crow::SimpleApp app;

    // Serve GUI
    CROW_ROUTE(app, "/")
    ([]() {
        crow::response r;
        r.set_static_file_info("static/index.html");
        return r;
    });

    // Connect to robot/simulator
    CROW_ROUTE(app, "/connect/<string>/<int>").methods(crow::HTTPMethod::POST)
    ([](const std::string& ip, int port) {
        std::lock_guard<std::mutex> lk(socketMtx);

        if (robotSocket) {
            delete robotSocket;
            robotSocket = nullptr;
        }

        try {
            robotIP = ip;
            robotPort = port;
            robotSocket = new MySocket(CLIENT, ip, port, UDP, 1024);
            pktCounter = 0;
            Log("Connected to " + ip + ":" + std::to_string(port));
            return crow::json::wvalue({{"status", "connected"},
                                        {"ip", ip}, {"port", port}});
        } catch (std::exception& e) {
            Log("Connect failed: " + std::string(e.what()));
            return crow::json::wvalue({{"error", std::string(e.what())}});
        }
    });

    // Telecommand (Drive / Sleep)
    CROW_ROUTE(app, "/telecommand/").methods(crow::HTTPMethod::PUT)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::json::wvalue({{"error", "Invalid JSON"}});

        std::string cmd = body["command"].s();
        PktDef pkt;

        if (cmd == "sleep") {
            pkt.SetCmd(SLEEP);
        } else if (cmd == "drive") {
            int dir = body["direction"].i();
            int duration = body["duration"].i();

            if (dir == LEFT || dir == RIGHT) {
                TurnBody t{};
                t.Direction = static_cast<unsigned char>(dir);
                t.Duration = static_cast<unsigned short>(duration);
                pkt.SetTurnBody(t);
            } else {
                int power = body["power"].i();
                DriveBody d{};
                d.Direction = static_cast<unsigned char>(dir);
                d.Duration = static_cast<unsigned char>(duration);
                d.Power = static_cast<unsigned char>(power);
                pkt.SetDriveBody(d);
            }
        } else {
            return crow::json::wvalue({{"error", "Unknown command"}});
        }

        return SendAndRecv(pkt);
    });

    // Telemetry request
    CROW_ROUTE(app, "/telementry_request/").methods(crow::HTTPMethod::GET)
    ([]() {
        PktDef pkt;
        pkt.SetCmd(RESPONSE);
        return SendAndRecv(pkt);
    });

    // Routing table
    CROW_ROUTE(app, "/routing_table/").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::json::wvalue({{"error", "Invalid JSON"}});
        // TODO: forward to another instance
        return crow::json::wvalue({{"status", "routing not yet implemented"}});
    });

    // Packet log endpoint
    CROW_ROUTE(app, "/log").methods(crow::HTTPMethod::GET)
    ([]() {
        std::lock_guard<std::mutex> lk(logMtx);
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> entries;
        for (auto& e : packetLog)
            entries.push_back(e);
        res["log"] = std::move(entries);
        return res;
    });

    std::cout << "Starting Command & Control GUI on http://0.0.0.0:8080" << std::endl;
    app.port(8080).multithreaded().run();
    
    delete robotSocket;
    return 0;
}
