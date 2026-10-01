---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: memcpy_h2n
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpyH2n.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [Struct Byte Access](../CAPLfunctionsStructByteAccessOverview.htm) Â» memcpy_h2n

# memcpy_h2n

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Function Syntax

void memcpy_h2n(byte dest[], struct source); // form 1

void memcpy_h2n(byte dest[], int offset, struct source); // form 2

## Description

Copies the bytes from the struct into the array, and translates the byte order of the elements from little-endian to big-endian (h2n stands for "host to network").

## Parameters

| source          | Struct whose bytes shall be copied         |
| --------------- | ------------------------------------------ |
| dest            | Array into which the bytes shall be copied |
| offset (form 2) | Offset into the array                      |

## Return Values

â

## Example

| ![Example](../../../../Resources/vImages/vExample.png "Example") | byte data[4];   struct WrapDword   {    dword dw;   } dwordWrapper;    int i;   for (i = 0; i < elcount(data); ++i)    data[i] = i;   memcpy\_n2h(dwordWrapper, data);   write("Bytes as dword: %0#10lx", dwordWrapper.dw);   dwordWrapper.dw = 0x12345678;   memcpy\_h2n(data, dwordWrapper);   write("dword as bytes: %#lx %#lx %#lx %#lx", data[0], data[1], data[2], data[3]); |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Availability

## [ClosedCANalyzer](javascript:void(0))

|                                     | CANalyzer        |
| ----------------------------------- | ---------------- |
| Since Version                       | < 12.0: form 1-2 |
| Restricted To                       | â              |
| Measurement Setup (Transmit Branch) | â              |
| Measurement Setup (Analysis Branch) | â              |
| 32-Bit                              | â              |
| 64-Bit                              | â              |

## [ClosedCANoe Desktop Editions](javascript:void(0))

|                             | CANoe DE         | CANoe MedTech DE | CANoe4SW:lite DE |
| --------------------------- | ---------------- | ---------------- | ---------------- |
| Since Version               | < 12.0: form 1-2 | 18: form 1-2     | 14: form 1-2     |
| Restricted To               | â              | â              | â              |
| Measurement Setup           | â              | N/A              | N/A              |
| Simulation Setup            | â              | â              | â              |
| Communication Setup         | â              | â              | â              |
| Test Setup for Test Modules | â              | â              | N/A              |
| Test Setup for Test Units   | â              | â              | â              |
| 32-Bit                      | â              | â              | â              |
| 64-Bit                      | â              | â              | â              |

## [ClosedCANoe Server Editions](javascript:void(0))

|                    | CANoe SE   (Windows) | CANoe SE   (Linux) | CANoe4SW SE   (Windows) | CANoe4SW SE   (Linux) | CANoe MedTech SE   (Windows) | CANoe MedTech SE   (Linux) |
| ------------------ | -------------------- | ------------------ | ----------------------- | --------------------- | ---------------------------- | -------------------------- |
| Since Version      | 18: form 1-2         | 18: form 1-2       | 13.0: form 1-2          | 16 SP3: form 1-2      | 19: form 1-2                 | 19: form 1-2               |
| Restricted To      | â                  | â                | â                     | â                   | â                          | â                        |
| Simulation Nodes   | â                  | â                | â                     | â                   | â                          | â                        |
| Application Models | â                  | â                | â                     | â                   | â                          | â                        |
| Test Units         | â                  | â                | â                     | â                   | â                          | â                        |
| 32-Bit             | â                  | N/A                | â                     | N/A                   | â                          | N/A                        |
| 64-Bit             | â                  | â                | â                     | â                   | â                          | â                        |

## [ClosedVector Test Unit Runner](javascript:void(0))

|               | Vector Test Unit Runner |
| ------------- | ----------------------- |
| Since Version | 17: form 1-2            |

## [ClosedvTESTstudio](javascript:void(0))

|               | vTESTstudio     |
| ------------- | --------------- |
| Since Version | < 4.0: form 1-2 |
| Restricted To | â             |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)