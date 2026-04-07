# COIL Robotics Project – Repository Audit Guide (implementation-focused, derived from official docs)”

## Purpose

This document is a repository audit and implementation guide derived from the official COIL project documents.

Its purpose is to help audit the repository, identify missing or incomplete requirements, and guide implementation and fixes across all milestones.

This guide is **not** a replacement for the official milestone documents.

## Source of Truth

All implementation, validation, fixes, and final requirement decisions must follow these documents:

- Robot Protocol Instructions v2.1 (Milestone 1)
- COIL Project Milestone 2
- COIL Project Milestone 3
- COIL Project Overview

If any conflict exists between this guide and the official documents, the official documents are the source of truth.

If any requirement in the official documents is unclear, inconsistent, or ambiguous:
- flag it explicitly
- do not silently invent behavior
- do not silently rename, reinterpret, or remove required items

---

# 1. SYSTEM GOAL

Build a webserver-based command-and-control GUI for robotic devices that:

- provides a browser-based GUI for controlling robotic devices
- encapsulates the robot application-layer protocol inside internet/webserver protocols
- supports commanding a robot through the webserver
- supports routing commands and telemetry to another instance of the same Command-and-Control GUI software
- logs incoming and outgoing packet information
- is configurable so the same software can be deployed on different platforms in the internal and international architecture

The project overview indicates deployment/configuration across:
- a local PC
- a Docker-based internal demonstration environment
- a Canadian VM
- a Netherlands VM

---

# 2. SYSTEM ARCHITECTURE

## Required High-Level Flow

Browser (HTTP)  
-> Webserver Application (Crow REST server)  
-> Socket Communication Layer (MySocket)  
-> Robot / Simulator

## Core Architectural Requirements

- The robot operates as a server using UDP socket command/response communication.
- The webserver acts as the middle manager between the browser and the robot.
- The webserver uses REST to receive commands from the GUI.
- The webserver must be capable of either:
  - commanding the robot through the socket connection, or
  - routing commands and telemetry to another instance in the network architecture

## Internal Demonstration Architecture

The software must support the internal demonstration arrangement where:
- the Command-and-Control GUI software runs in a Docker container on one PC
- that PC is connected to both the private robot network and CCSecure Wi-Fi
- a second PC uses a web browser to load and operate the GUI

## International Demonstration Architecture

The project overview indicates deployment/configuration such that instances can run on:
- one local PC
- one Canadian VM
- one Netherlands VM

The software must support the broader routing/operation model required for that architecture.

---

# 3. MILESTONE 1 – `PktDef` CLASS

## Goal

Create a C++ class named `PktDef` that defines and implements the robot application-layer protocol.

This class is used to:
- create packets to send to the robot
- parse packets received from the robot
- serialize packet data into a raw transmit buffer
- validate packet integrity using the required CRC/parity method

Testing must be done using MSTest.

---

## Packet Structure

Each packet contains:
- Header
- Body (dynamic / variable length)
- Trailer

---

## Header Definition

The packet header contains:
- `PktCount`
- command flags:
  - `Drive`
  - `Status`
  - `Sleep`
  - `Ack`
- padding bits
- `Length`

### Header Field Notes

- `PktCount` is 2 bytes.
- The four command indicators are bit-level flags.
- Padding occupies 4 bits.
- The documents contain an ambiguity regarding `Length`:
  - the written description says `Length` contains an unsigned short int with the total number of bytes in the packet
  - the protocol size diagram shows `Length` as 1 byte

### Required Handling of the `Length` Ambiguity

Do not silently change the interpretation of `Length`.

If the repository already implements one interpretation, or simulator behavior indicates one interpretation, verify that behavior before changing it.

If this ambiguity affects implementation or compatibility:
- flag it explicitly
- preserve document and simulator compatibility
- do not invent undocumented behavior

### Header Flag Rules

- Drive, Status, and Sleep should never be set at the same time.
- Ack should always be set with a corresponding command flag.
- The `Status` flag is used for response/telemetry packets as described in the protocol document.

---

## Body Definitions

### `DriveBody`

Used for forward/backward drive commands.

Fields:
- `Direction` = 1 byte
- `Duration` = 1 byte
- `Power` = 1 byte

Rules:
- Power must be between 80 and 100 inclusive.

### `TurnBody`

Used for left/right drive commands.

Fields:
- `Direction` = 1 byte
- `Duration` = 2 bytes

Rules:
- Turning is always performed using 100% power.
- There is no separate explicit power field in `TurnBody`.

### Sleep and Response Commands

- Sleep and Response commands have no body parameters.

---

## Direction Constants

The following constant values are required:

```cpp
FORWARD  = 1
BACKWARD = 2
RIGHT    = 3
LEFT     = 4
Trailer Definition

The trailer contains:

CRC = 1 byte
CRC / Parity Algorithm

The required 1-byte CRC is a parity-style bit count:

count the total number of bits set to 1 in the packet contents
store that count as the 1-byte CRC value

This CRC must be used for:

packet generation
packet validation
Response Packet Types
ACK Packet

An acknowledgement packet:

has Ack = 1
has the corresponding command bit set
may optionally include a message string in the body
NACK Packet

A negative acknowledgement packet:

has Ack = 0
has the corresponding command bit set
may optionally include a message string in the body
Telemetry Response Packet

A telemetry response:

uses the same header/trailer format
has Status = 1
includes housekeeping telemetry data in the body

Telemetry body fields:

LastPktCounter (unsigned short int)
CurrentGrade (unsigned short int)
HitCount (unsigned short int)
Heading (unsigned short int)
LastCmd (unsigned char)
LastCmdValue (unsigned char)
LastCmdPower (unsigned char)
Heading Note

The protocol document marks Heading as experimental and indicates it will be populated based on turn duration.

Required Structures

The following structures must exist:

Header
DriveBody
TurnBody
Required Enum

The following enum must exist:

enum CmdType {
  DRIVE,
  SLEEP,
  RESPONSE
};
Required Constants

The following constants must exist:

FORWARD
BACKWARD
LEFT
RIGHT
HEADERSIZE
HEADERSIZE Rule

HEADERSIZE represents the size of the header in bytes and must be calculated manually from the protocol definition.

Required PktDef Internal Members

The class must contain, at minimum:

a private structure to define a command packet
Header
char* Data
char CRC
char* RawBuffer

RawBuffer must store the serialized packet representation used for transmission.

Required Functions

The class must contain, at minimum, the following member functions:

Constructors
PktDef()
PktDef(char*)
PktDef()

Default constructor safe state requirements:

all header information set to zero
Data = nullptr
CRC = 0
PktDef(char*)
parses a raw data buffer
populates header, body, and CRC contents
Setters
void SetCmd(CmdType)
void SetBodyData(char*, int)
void SetPktCount(int)
Getters / Query Functions
CmdType GetCmd()
bool GetAck()
int GetLength()
char* GetBodyData()
int GetPktCount()
CRC Functions
bool CheckCRC(char*, int)
void CalcCRC()
Packet Generation
char* GenPacket()

GenPacket() must allocate and populate RawBuffer and return the address of the serialized packet.

Milestone 1 Testing Requirements
Must use MSTest.
Test cases must cover the required functionality of PktDef.
Testing quality matters.
Audit Checks for Milestone 1

At minimum, verify:

PktDef class exists
required structs, enum, constants, and members exist
constructors and required member functions exist
packet creation works
packet parsing works
raw serialization works
CRC calculation follows the required bit-count method
CRC validation is implemented
command flags are handled correctly
ACK/NACK behavior is handled
telemetry response handling is present if implemented or required by the repo flow
MSTest project exists
MSTest coverage addresses required functionality

If any item cannot be proven from the repository, mark it as missing, ambiguous, or unverified instead of assuming pass.

4. MILESTONE 2 – MySocket CLASS
Goal

Create a class named MySocket that defines and implements TCP and UDP socket communications.

A MySocket object is responsible for:

configuration of a single socket connection
communication over that connection
carrying application-layer data in raw byte form
operating as either a client or server

Testing must be done using MSTest.

Required Global Definitions

The following must exist in the global namespace:

enum SocketType { CLIENT, SERVER };
enum ConnectionType { TCP, UDP };
const int DEFAULT_SIZE = /* required constant */;
Required Member Variables

The MySocket class must contain, at minimum:

char* Buffer
WelcomeSocket
ConnectionSocket
struct sockaddr_in SvrAddr
SocketType mySocket
std::string IPAddr
int Port
ConnectionType connectionType
bool bTCPConnect
int MaxSize
Member Variable Meaning
Buffer is dynamically allocated raw buffer space
WelcomeSocket is used by a TCP server
ConnectionSocket is used for client/server communications for TCP and UDP
SvrAddr stores connection information
MaxSize is used to help prevent overflows and synchronization issues
Required Constructor
MySocket(SocketType, std::string, unsigned int, ConnectionType, unsigned int)

The constructor must:

configure socket type and connection type
set IP address and port number
dynamically allocate Buffer
use DEFAULT_SIZE if an invalid size is provided
put servers into a condition to:
accept connections if TCP
receive messages if UDP
Required Destructor
~MySocket()

The destructor must clean up dynamically allocated memory.

Required Core Functions
void ConnectTCP()
void DisconnectTCP()
void SendData(const char*, int)
int GetData(char*)
Function Behavior Notes
ConnectTCP()
establishes a TCP connection
should prevent a UDP-configured object from initiating a connection-oriented scenario
DisconnectTCP()
disconnects an established TCP connection
SendData(const char*, int)
sends a raw block of bytes over the socket
must work for both TCP and UDP
GetData(char*)
receives raw data into the internal buffer
copies received data to the provided memory address
returns the total number of bytes written
must work for both TCP and UDP
Required Getters / Setters
std::string GetIPAddr()
void SetIPAddr(std::string)
int GetPort()
void SetPort(int)
SocketType GetType()
void SetType(SocketType)
Required Guard Rules
SetIPAddr(std::string)
must return or report an error if a connection has already been established
SetPort(int)
must return or report an error if a connection has already been established
SetType(SocketType)
must prevent type changes if:
a TCP connection is established, or
the welcome socket is open
Milestone 2 Testing Requirements
Must use MSTest.
Test cases must cover the required functionality of MySocket.
Testing quality matters.
Audit Checks for Milestone 2

At minimum, verify:

MySocket class exists
required enums, constant, members, constructor, destructor, getters, setters, and socket functions exist
invalid buffer size falls back to DEFAULT_SIZE
TCP and UDP behavior is implemented
client and server modes are supported
setter guard logic is present
MSTest project exists
MSTest coverage addresses required functionality

If any item cannot be proven from the repository, mark it as missing, ambiguous, or unverified instead of assuming pass.

5. MILESTONE 3 – WEB SERVER APPLICATION
Goal

Integrate PktDef and MySocket into a RESTful webserver application that acts as the Command-and-Control GUI for the robotic devices.

The webserver must:

provide a browser GUI
receive commands through REST routes
control the robot through the socket layer
or route commands/telemetry to another instance in the architecture
log all incoming and outgoing packet information
Required Technology
Crow must be used as the webserver framework.
The application must be RESTful.
The application acts as the middle manager between browser and robot.
Required Webserver Responsibilities

The webserver must, at minimum:

provide a GUI control for operation from a browser
provide the ability to connect and route commands/telemetry directly to a connected robot
provide the ability to route commands/telemetry to another instance of the Command-and-Control GUI software
log all incoming and outgoing packet information while running
Required Routes

The following routes are required and should be treated as mandatory.

/
root route
returns the main Command-and-Control GUI
/connect
must use POST only
must take two parameters as part of the route definition:
<string> for robot/simulator IP address
<int> for robot/simulator port
sets the internal communication parameters for UDP/IP communication
/telecommand/
must use PUT only
sends a Drive or Sleep style command to the robot
/telementry_request/
must use GET only
spelling must remain exactly:
/telementry_request/
sends the request for housekeeping telemetry command to the robot
/routing_table/
used to route commands and telemetry to a different instance in the network architecture
GUI Display Requirements

Because this is a command/response system, the GUI must display, at minimum:

acknowledgement information
housekeeping telemetry information

For audit purposes, it is reasonable to verify handling/display of:

ACK responses
NACK responses
telemetry data
Logging Requirement

The webserver must log:

all incoming packet information
all outgoing packet information

Logging must not be omitted.

6. SIMULATOR REQUIREMENTS

The solution must work with:

Robot_Simulator.exe

The simulator:

runs as a Windows console application
accepts a port number argument
listens on any IP by default
validates received commands
displays an English interpretation of the command
responds with ACK or NACK packets
Required Demonstrated Operations

The solution must be demonstrated working with the simulator for:

Drive Forward
Drive Backward
Drive Left
Drive Right
Request Housekeeping Telemetry
Sleep
7. DEPLOYMENT REQUIREMENTS
Internal Demonstration

The system must support the internal demonstration setup where:

the software runs in a Docker container on one PC
that PC is connected to the robot network and CCSecure
a second PC uses a browser to operate the GUI
International Architecture

The system must be configurable/deployable for use as:

one instance on a local PC
one instance on a Canadian VM
one instance on a Netherlands VM

The software must support the broader architecture and routing behavior required for that deployment model.

8. SUBMISSION REQUIREMENTS
Milestone 1 Submission

Must include:

Visual Studio project files
README.txt with execution instructions if required
any required input/test files
debug, release, and .vs directories removed before submission
Milestone 2 Submission

Must include:

Visual Studio project files
README.txt with execution instructions if required
any required input/test files
debug, release, and .vs directories removed before submission
Milestone 3 Submission

Must include:

all HTML, CSS, Javascript, and C/C++ source code
README.txt with execution instructions if required
any required input/test files

The Milestone 3 document also requires a copy of the testing video demonstrating simulator interaction.

Milestone 4
no submission package
this is the demo/competition day
9. REPOSITORY AUDIT CHECKLIST

Use the following checklist to audit the repository against the official documents.

General
Web-based command-and-control GUI exists
Browser-to-webserver control path exists
Packet logging is implemented
Direct robot communication is supported
Routing to another instance is supported
Configurable deployment is supported
README or execution instructions are included where required
PktDef
PktDef class exists
Header structure exists
DriveBody structure exists
TurnBody structure exists
CmdType { DRIVE, SLEEP, RESPONSE } exists
FORWARD, BACKWARD, LEFT, RIGHT constants exist with correct values
HEADERSIZE exists and is manually calculated
private packet structure exists
Header, Data, CRC, and RawBuffer members are present
default constructor sets safe state
raw-buffer parsing constructor exists
SetCmd() implemented
SetBodyData() implemented
SetPktCount() implemented
GetCmd() implemented
GetAck() implemented
GetLength() implemented
GetBodyData() implemented
GetPktCount() implemented
CheckCRC() implemented
CalcCRC() implemented
GenPacket() implemented
header fields handled correctly
command flag exclusivity handled correctly
Ack pairing handled correctly
DriveBody handled correctly
TurnBody handled correctly
Power 80–100 enforcement handled correctly for drive commands
CRC implemented using required bit-count method
ACK/NACK handling present
telemetry response handling present where required by implementation flow
raw packet generation works
raw packet parsing works
MSTest project exists
MSTest coverage addresses required functionality
MySocket
MySocket class exists
SocketType { CLIENT, SERVER } exists
ConnectionType { TCP, UDP } exists
DEFAULT_SIZE exists
dynamic buffer allocation implemented
WelcomeSocket present
ConnectionSocket present
SvrAddr present
mySocket present
IPAddr present
Port present
connectionType present
bTCPConnect present
MaxSize present
constructor implemented correctly
invalid size falls back to DEFAULT_SIZE
TCP server preparation implemented
UDP server preparation implemented
destructor implemented
ConnectTCP() implemented
DisconnectTCP() implemented
SendData() implemented
GetData() implemented
GetIPAddr() implemented
SetIPAddr() implemented with required guard logic
GetPort() implemented
SetPort() implemented with required guard logic
GetType() implemented
SetType() implemented with required guard logic
TCP and UDP supported
client and server modes supported
MSTest project exists
MSTest coverage addresses required functionality
Webserver
Crow is used
RESTful design is present
/ route implemented
/connect POST route implemented
/connect accepts IP and Port route parameters
/telecommand/ PUT route implemented
/telementry_request/ GET route implemented with exact spelling
/routing_table/ implemented
GUI served from root route
GUI displays acknowledgement information
GUI displays housekeeping telemetry information
direct robot communication path works
routing-to-another-instance path exists
incoming packet logging implemented
outgoing packet logging implemented
Simulator / Demo
works with Robot_Simulator.exe
simulator port configuration supported
Forward demonstrated
Backward demonstrated
Left demonstrated
Right demonstrated
telemetry request demonstrated
Sleep demonstrated
GUI output visible during demo
simulator console output visible during demo
testing/demo video available if auditing Milestone 3 submission completeness
10. CURSOR AUDIT AND IMPLEMENTATION RULES

You must follow these rules when auditing or completing the repository.

Before Coding
Audit the repository against the official documents and this checklist.
List every missing, incomplete, incorrect, ambiguous, or unverified requirement.
Do not assume a feature exists unless the repository proves it.
Do not treat this markdown as above the official documents.
During Coding
Do not remove required features.
Do not rename required classes, enums, constants, functions, or routes unless the official documents explicitly allow it.
Do not skip:
CRC/parity logic
packet logging
ACK/NACK handling
telemetry handling where required
MSTest requirements for Milestones 1 and 2
Preserve simulator compatibility.
Preserve required route names exactly where specified.
If a behavior is ambiguous in the documents, flag it instead of silently inventing a change.
After Coding

Re-audit the repository and report each audited item as one of:

PASS
FAIL
AMBIGUOUS
UNVERIFIED
Reporting Rules
PASS only if the repository clearly proves the requirement is satisfied.
FAIL if the repository clearly does not satisfy the requirement.
AMBIGUOUS if the official documents themselves are inconsistent or unclear.
UNVERIFIED if the requirement may be satisfied, but the repository does not provide enough evidence.

Clearly identify:

completed items
missing items
ambiguous items
items blocked by missing evidence
11. FINAL RULE

Do not invent behavior.

If something is unclear, inconsistent, or ambiguous:

flag it explicitly
verify against the official documents, simulator expectations, and existing implementation behavior
do not silently change protocol details, route names, packet structure, or required interfaces

One small correction from your original: keep `RIGHT = 3` and `LEFT = 4`, and make sure that code 

---

## 12. ADDITIONS TO ENSURE FULLER DOCUMENT COVERAGE

The sections above focus primarily on repository-auditable implementation requirements.  
The official documents also include the following important project-level requirements, behavioral requirements, and architectural clarifications that should not be ignored during audit or implementation review.

### A. ROBOT COMMUNICATION MODEL CLARIFICATION

The robot itself operates as a **UDP server** for command/response communication.

This distinction must remain explicit during audit:
- robot-facing command/response communication is UDP-based
- broader architecture and inter-instance routing may involve other transport behavior depending on deployment design
- do not assume that because `MySocket` supports both TCP and UDP, the physical robot interface may be changed away from the required UDP command/response model

### B. ROBOT OPERATING MODES

The robot has three defined modes of operation:

- **Waiting**
  - default mode after power on
  - waits for command packets from the Command-and-Control software

- **Drive**
  - entered when a valid drive command is received
  - robot validates the command
  - robot responds with an ACK packet
  - robot executes the drive action
  - robot returns to Waiting mode after completing the action

- **Sleep**
  - entered when a valid sleep command is received
  - robot validates the command
  - robot responds with an ACK packet
  - robot resets all internal counters and housekeeping telemetry status to zero

Where repository behavior, simulator behavior, or UI expectations depend on these states, they must be preserved.

### C. MULTI-ROBOT / MULTI-DEVICE SCOPE

This project is not only about controlling a single robot.

The official documents describe the software as a Command-and-Control GUI for:
- **multiple tele-operated robotic devices**
- command and routing scenarios involving more than one robotic endpoint or instance in the network architecture

During audit, do not treat a single hardcoded robot target as automatically sufficient if the repository design cannot reasonably be configured or extended to support multiple robotic devices or routed instances.

### D. INTERNAL DEMONSTRATION ENVIRONMENT CLARIFICATION

For the internal demonstration:
- the software must be configured and running in a **Docker container** on one PC
- that PC will be connected to:
  - the private robot network
  - Conestoga’s CCSecure Wi-Fi
- a second PC will use a web browser to load and operate the GUI

This Docker requirement should be treated as a concrete operational requirement for demonstration readiness, not merely a suggestion.

### E. DEPLOYMENT TARGET CLARIFICATION

The project overview states that the software is intended to be usable through configuration on multiple deployment targets.

These targets include:
- one instance on a local PC
- one instance on the Canadian VM
- one instance on the Netherlands VM

Keep this separate from the internal Docker demonstration requirement:
- Docker is part of the internal demonstration setup
- local PC / Canadian VM / Netherlands VM are deployment/configuration targets in the broader architecture

### F. COLLABORATION REQUIREMENTS FROM THE OFFICIAL DOCUMENTS

The official documents state that collaboration with Fontys University students is part of the project expectations.

This includes:
- discussing infrastructure requirements
- discussing potential transmission/security restrictions
- continuing collaboration through the Discord collaboration space

This is not strictly a repository-code requirement, but it is part of the documented project requirements and should be acknowledged as a non-code project obligation.

### G. NON-CODE PROJECT REQUIREMENTS NOT DIRECTLY AUDITABLE FROM THE REPOSITORY

The official documents also include project-level requirements that may not be directly provable from source code alone, including:
- group-based execution
- group size requirements
- internal competition / demonstration procedures
- peer evaluation / judging context
- top-team deployment to the international infrastructure

These items should not be marked PASS based only on repository evidence.  
If auditing strictly from source code, mark such items as:
- **NOT REPOSITORY-AUDITABLE**, or
- **UNVERIFIED FROM REPOSITORY ALONE**

### H. AUDIT HANDLING RULE FOR NON-REPOSITORY REQUIREMENTS

When auditing the repository, distinguish between:
- implementation requirements provable from code and project files
- operational requirements provable from Docker/configuration/demo assets
- non-code course/project requirements that cannot be proven from the repository alone

Do not falsely mark non-code project requirements as satisfied just because the repository looks complete.

### I. FINAL COMPLETENESS RULE

This document is an implementation-focused audit guide derived from the official documents.

It is intended to cover repository-auditable requirements as completely as possible, but it does **not** replace the official milestone documents for:
- course process rules
- submission workflow rules
- group/project administration rules
- demo-day judging procedures
- collaboration expectations outside the codebase

If any uncertainty remains, verify directly against the official documents before making a final pass/fail decision.