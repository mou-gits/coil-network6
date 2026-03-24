This document contains all diagrams related to the design and implementation of the `PktDef` class and the robot communication protocol.
These diagrams support the understanding of packet structure, system behavior, and data flow.

---

#1. Packet Structure Diagram

```mermaid
flowchart LR
    A["Packet"] --> H["Header"]
    A --> B["Body"]
    A --> C["CRC (1 byte)"]

    H --> H1["PktCount (2 bytes)"]
    H --> H2["Flags (1 byte total)"]
    H --> H3["Length (1 byte)"]

    H2 --> F1["Drive (1 bit)"]
    H2 --> F2["Status (1 bit)"]
    H2 --> F3["Sleep (1 bit)"]
    H2 --> F4["Ack (1 bit)"]
    H2 --> F5["Padding (4 bits)"]

    B --> D1["DriveBody\nDirection (1 byte)\nDuration (1 byte)\nPower (1 byte)"]
    B --> D2["TurnBody\nDirection (1 byte)\nDuration (2 bytes)"]
    B --> D3["Empty Body\n(Sleep / Response without params)"]

    note1["Body depends on command type:
    - DRIVE forward/backward -> DriveBody
    - DRIVE left/right -> TurnBody
    - SLEEP -> Empty
    - RESPONSE/ACK/NACK may contain optional message or telemetry"]
```

---

#2. PktDef Class Diagram

```mermaid
classDiagram
    class PktDef {
        - CmdPacket packet
        - char* RawBuffer

        + PktDef()
        + PktDef(char* rawData)

        + void SetCmd(CmdType cmd)
        + void SetBodyData(char* data, int size)
        + void SetPktCount(int count)

        + CmdType GetCmd()
        + bool GetAck()
        + int GetLength()
        + char* GetBodyData()
        + int GetPktCount()

        + bool CheckCRC(char* data, int size)
        + void CalcCRC()
        + char* GenPacket()
    }

    class Header {
        + unsigned short PktCount
        + unsigned char Drive
        + unsigned char Status
        + unsigned char Sleep
        + unsigned char Ack
        + unsigned char Padding
        + unsigned char Length
    }

    class DriveBody {
        + unsigned char Direction
        + unsigned char Duration
        + unsigned char Power
    }

    class TurnBody {
        + unsigned char Direction
        + unsigned short Duration
    }

    class CmdPacket {
        + Header header
        + char* Data
        + char CRC
    }

    class CmdType {
        <<enumeration>>
        DRIVE
        SLEEP
        RESPONSE
    }

    PktDef *-- CmdPacket
    CmdPacket *-- Header
    PktDef ..> DriveBody
    PktDef ..> TurnBody
    PktDef ..> CmdType
```

---

#3. Serialization / Deserialization Flow Diagram

```mermaid
flowchart TD
    A["Start"] --> B{"Operation Type?"}

    B -->|Serialization| C["Create / populate Header"]
    C --> D["Attach Body data\n(DriveBody / TurnBody / Empty)"]
    D --> E["Calculate CRC"]
    E --> F["Build RawBuffer\nHeader -> Body -> CRC"]
    F --> G["Packet ready for transmission"]

    B -->|Deserialization| H["Receive RawBuffer"]
    H --> I["Parse Header"]
    I --> J["Determine command/body type"]
    J --> K["Parse Body"]
    K --> L["Read CRC"]
    L --> M{"CRC valid?"}
    M -->|Yes| N["Populate PktDef object"]
    M -->|No| O["Reject packet / mark invalid"]
```

---

#4. Sequence Diagram (Build → Serialize → Parse)

```mermaid
sequenceDiagram
    participant U as User/Test
    participant P1 as PktDef (sender)
    participant R as RawBuffer
    participant P2 as PktDef (receiver/parser)

    U->>P1: Create PktDef()
    U->>P1: SetCmd(CmdType)
    U->>P1: SetPktCount(count)
    U->>P1: SetBodyData(data, size)
    U->>P1: CalcCRC()
    U->>P1: GenPacket()

    P1-->>R: Serialized packet\n[Header | Body | CRC]

    U->>P2: Create PktDef(RawBuffer)
    P2->>P2: Parse Header
    P2->>P2: Parse Body
    P2->>P2: CheckCRC(rawData, size)
    P2-->>U: Parsed packet + CRC validation result
```
-----

## 📌 Notes

- Diagrams are intentionally simplified for clarity
- They directly support Milestone 1 implementation
- All diagrams are created using Mermaid syntax
