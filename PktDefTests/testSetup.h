#pragma once

// MUST come first — this defines the primary template
#include "CppUnitTest.h"
#include <string>
#include "../PktDefLib/PktDef.h"

// Now the specialization is legal
namespace Microsoft {
    namespace VisualStudio {
        namespace CppUnitTestFramework
        {
            template<>
            inline std::wstring ToString<PacketClass>(const PacketClass& value)
            {
                switch (value)
                {
                case PacketClass::Invalid:             return L"Invalid";
                case PacketClass::DriveCommand:        return L"DriveCommand";
                case PacketClass::TurnCommand:         return L"TurnCommand";
                case PacketClass::SleepCommand:        return L"SleepCommand";
                case PacketClass::TelemetryCommand:    return L"TelemetryCommand";
                case PacketClass::AckResponse:         return L"AckResponse";
                case PacketClass::NAckResponse:        return L"NAckResponse";
                case PacketClass::TelemetryResponse:   return L"TelemetryResponse";
                default:                               return L"(Unknown PacketClass)";
                }
            }
        }
    }
}
