---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: memcpy
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpy.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [Struct Byte Access](../CAPLfunctionsStructByteAccessOverview.htm) Â» memcpy

# memcpy

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Function Syntax

void memcpy(byte dest[], struct \* source); // form 1

void memcpy(char dest[], struct \* source); // form 2

void memcpy(byte dest[], long offset, struct \* source); // form 3

void memcpy(char dest[], long offset, struct \* source); // form 4

void memcpy(struct \* dest, byte source[]); // form 5

void memcpy(struct \* dest, char source[]); // form 6

void memcpy(struct \* dest, byte source[], long offset); // form 7

void memcpy(struct \* dest, char source[], long offset); // form 8

void memcpy(struct \* dest, struct \* source); // form 9

void memcpy(byte dest[], byte source[], dword length); // form 10

void memcpy(byte dest[], char source[], dword length); // form 11

void memcpy(char dest[], byte source[], dword length); // form 12

void memcpy(char dest[], char source[], dword length); // form 13

void memcpy(struct dest, char source[]); // form 14

void memcpy(struct dest, byte source[]); // form 15

void memcpy(char dest[], struct source); // form 16

void memcpy(byte dest[], struct source); // form 17

void memcpy(PDUPayload dest, PDUPayload source, dword length); // form 18

void memcpy(PDUPayload dest, byte source[], dword length); // form 19

void memcpy(byte source[], PDUPayload dest, dword length); // form 20

void memcpy(PDUPayload dest, struct \* source); // form 21

void memcpy(struct \* dest, PDUPayload source); // form 22

void memcpy(bytes dest, byte source[]); // form 23

void memcpy(bytes dest, char source[]); // form 24

void memcpy(bytes dest, byte source[], dword length); // form 25

void memcpy(bytes dest, char source[], dword length); // form 26

void memcpy(byte dest[], bytes source); // form 27

void memcpy(char dest[], bytes source); // form 28

## Description

Copies bytes from a source to a destination. In form 9, both structs must have the same type. In other forms with structs, the arrays must be large enough to contain the struct data. In forms 18 to 22, the payload size and the struct size must be identical.

## Parameters

| source | (form 1-4, 9, 16, 17, 21): Struct whose bytes shall be copied.<br>(form 18, 22): Payload data of a PDU whose bytes shall be copied.<br>(form 27, 28): vCDL "bytes" value that shall be copied.<br>(other forms): Array whose bytes shall be copied. |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| dest   | (form 5, 6, 7, 8, 9, 22): Struct into which the bytes shall be copied.<br> (form 18, 19, 20, 21): Payload data of a PDU into which the bytes shall be copied.<br>(form 23, 24, 25, 26): vCDL "bytes" value into which the bytes shall be copied.<br>(other forms): Array into which the bytes shall be copied. |
| offset | (form 3, 4, 7, 8): Offset in the array which marks the start of the data.                                                                                                                                |
| length | (form 10, 11, 12, 13, 18, 19, 20, 25, 26): number of bytes which shall be copied.                                                                                                                        |

## Return Values

â

## Example

| ![Example](../../../../Resources/vImages/vExample.png "Example") | byte data[4];   struct WrapDword   {    dword dw;   } dwordWrapper;    int i;   for (i = 0; i < elcount(data); ++i)    data[i] = i;   memcpy(dwordWrapper, data);   write("Bytes as dword: %0#10lx", dwordWrapper.dw);   dwordWrapper.dw = 0x12345678;   memcpy(data, dwordWrapper);   write("dword as bytes: %#lx %#lx %#lx %#lx", data[0], data[1], data[2], data[3]); |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Availability

## [ClosedCANalyzer](javascript:void(0))

|                                     | CANalyzer                           |
| ----------------------------------- | ----------------------------------- |
| Since Version                       | < 12.0: form 1-22<br>15: form 23-28 |
| Restricted To                       | â                                 |
| Measurement Setup (Transmit Branch) | â                                 |
| Measurement Setup (Analysis Branch) | â                                 |
| 32-Bit                              | â                                 |
| 64-Bit                              | â                                 |

## [ClosedCANoe Desktop Editions](javascript:void(0))

|                             | CANoe DE                            | CANoe MedTech DE | CANoe4SW:lite DE                |
| --------------------------- | ----------------------------------- | ---------------- | ------------------------------- |
| Since Version               | < 12.0: form 1-22<br>15: form 23-28 | 18: form 1-28    | 14: form 1-22<br>15: form 23-28 |
| Restricted To               | â                                 | â              | â                             |
| Measurement Setup           | â                                 | N/A              | N/A                             |
| Simulation Setup            | â                                 | â              | â                             |
| Communication Setup         | â                                 | â              | â                             |
| Test Setup for Test Modules | â                                 | â              | N/A                             |
| Test Setup for Test Units   | â                                 | â              | â                             |
| 32-Bit                      | â                                 | â              | â                             |
| 64-Bit                      | â                                 | â              | â                             |

## [ClosedCANoe Server Editions](javascript:void(0))

|                    | CANoe SE   (Windows) | CANoe SE   (Linux) | CANoe4SW SE   (Windows)         | CANoe4SW SE   (Linux) | CANoe MedTech SE   (Windows) | CANoe MedTech SE   (Linux) |
| ------------------ | -------------------- | ------------------ | ------------------------------- | --------------------- | ---------------------------- | -------------------------- |
| Since Version      | 18: form 1-28        | 18: form 1-28      | 13: form 1-22<br>15: form 23-28 | 16 SP3: form 1-28     | 19: form 1-28                | 19: form 1-28              |
| Restricted To      | â                  | â                | â                             | â                   | â                          | â                        |
| Simulation Nodes   | â                  | â                | â                             | â                   | â                          | â                        |
| Application Models | â                  | â                | â                             | â                   | â                          | â                        |
| Test Units         | â                  | â                | â                             | â                   | â                          | â                        |
| 32-Bit             | â                  | N/A                | â                             | N/A                   | â                          | N/A                        |
| 64-Bit             | â                  | â                | â                             | â                   | â                          | â                        |

## [ClosedVector Test Unit Runner](javascript:void(0))

|               | Vector Test Unit Runner |
| ------------- | ----------------------- |
| Since Version | 17: form 1-28           |

## [ClosedvTESTstudio](javascript:void(0))

|               | vTESTstudio                       |
| ------------- | --------------------------------- |
| Since Version | < 4.0: form 1-22<br>6: form 23-28 |
| Restricted To | â                               |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)