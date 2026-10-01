---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: memcpy_off
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpy_off.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [Struct Byte Access](../CAPLfunctionsStructByteAccessOverview.htm) Â» memcpy_off

# memcpy_off

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Function Syntax

void memcpy_off( struct type dest, dword destOffset, byte source[], dword sourceOffset, dword length); // form 1

void memcpy_off( struct type dest, dword destOffset, char source[], dword sourceOffset, dword length); // form 2

void memcpy_off( byte dest[], dword destOffset, struct type source, dword sourceOffset, dword length); // form 3

void memcpy_off( char dest[], dword destOffset, struct type source, dword sourceOffset, dword length); // form 4

void memcpy_off( byte dest[], dword destOffset, byte source[], dword sourceOffset, dword length); // form 5

void memcpy_off( char dest[], dword destOffset, byte source[], dword sourceOffset, dword length); // form 6

void memcpy_off( byte dest[], dword destOffset, char source[], dword sourceOffset, dword length); // form 7

void memcpy_off( char dest[], dword destOffset, char source[], dword sourceOffset, dword length); // form 8

void memcpy_off( struct dest, dword destOffset, char source[], dword sourceOffset, dword length); // form 9

void memcpy_off( union dest, dword destOffset, char source[], dword sourceOffset, dword length); // form 10

void memcpy_off( array dest, dword destOffset, char source[], dword sourceOffset, dword length); // form 11

void memcpy_off( byte[] dest, dword destOffset, struct source, dword sourceOffset, dword length); // form 12

void memcpy_off( byte[] dest, dword destOffset, union source, dword sourceOffset, dword length); // form 13

void memcpy_off( byte[] dest, dword destOffset, array source, dword sourceOffset, dword length); // form 14

## Description

Copies bytes from a source to destination, giving a destination start offset. The size of the destination must be at least destOffset + length.

## Parameters

| dest         | (form 1, 2): Struct into which the bytes shall be copied.<br>(form, 9, 10, 11): vCDL object into which the bytes shall be copied.   (other forms): Array into which the bytes shall be copied. |
| ------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| source       | (form 3, 4): Struct from which the bytes shall be copied.<br>(form 12, 13, 14): vCDL object from which the bytes shall be copied.   (other forms): Array from which the bytes shall be copied. |
| destOffset   | Start offset in the destination struct or array.                                                                                                                                               |
| sourceOffset | Start offset int the source struct or array.                                                                                                                                                   |
| length       | Number of bytes which shall be copied.                                                                                                                                                         |

## Return Values

â

## Example

â

## Availability

## [ClosedCANalyzer](javascript:void(0))

|                                     | CANalyzer                             |
| ----------------------------------- | ------------------------------------- |
| Since Version                       | < 12.0: form 1-8<br>17 SP3: form 9-14 |
| Restricted To                       | â                                   |
| Measurement Setup (Transmit Branch) | â                                   |
| Measurement Setup (Analysis Branch) | â                                   |
| 32-Bit                              | â                                   |
| 64-Bit                              | â                                   |

## [ClosedCANoe Desktop Editions](javascript:void(0))

|                             | CANoe DE                              | CANoe MedTech DE | CANoe4SW:lite DE                  |
| --------------------------- | ------------------------------------- | ---------------- | --------------------------------- |
| Since Version               | < 12.0: form 1-8<br>17 SP3: form 9-14 | 18: form 1-14    | 14: form 1-8<br>17 SP3: form 9-14 |
| Restricted To               | â                                   | â              | â                               |
| Measurement Setup           | â                                   | N/A              | N/A                               |
| Simulation Setup            | â                                   | â              | â                               |
| Communication Setup         | â                                   | â              | â                               |
| Test Setup for Test Modules | â                                   | â              | N/A                               |
| Test Setup for Test Units   | â                                   | â              | â                               |
| 32-Bit                      | â                                   | â              | â                               |
| 64-Bit                      | â                                   | â              | â                               |

## [ClosedCANoe Server Editions](javascript:void(0))

|                    | CANoe SE   (Windows) | CANoe SE   (Linux) | CANoe4SW SE   (Windows)             | CANoe4SW SE   (Linux)                 | CANoe MedTech SE   (Windows) | CANoe MedTech SE   (Linux) |
| ------------------ | -------------------- | ------------------ | ----------------------------------- | ------------------------------------- | ---------------------------- | -------------------------- |
| Since Version      | 18: form 1-14        | 18: form 1-14      | 13.0: form 1-8<br>17 SP3: form 9-14 | 16 SP3: form 1-8<br>17 SP3: form 9-14 | 19: form 1-14                | 19: form 1-14              |
| Restricted To      | â                  | â                | â                                 | â                                   | â                          | â                        |
| Simulation Nodes   | â                  | â                | â                                 | â                                   | â                          | â                        |
| Application Models | â                  | â                | â                                 | â                                   | â                          | â                        |
| Test Units         | â                  | â                | â                                 | â                                   | â                          | â                        |
| 32-Bit             | â                  | N/A                | â                                 | N/A                                   | â                          | N/A                        |
| 64-Bit             | â                  | â                | â                                 | â                                   | â                          | â                        |

## [ClosedVector Test Unit Runner](javascript:void(0))

|               | Vector Test Unit Runner |
| ------------- | ----------------------- |
| Since Version | 17: form 1-14           |

## [ClosedvTESTstudio](javascript:void(0))

|               | vTESTstudio                         |
| ------------- | ----------------------------------- |
| Since Version | < 4.0: form 1-8<br>8 SP3: form 9-14 |
| Restricted To | â                                 |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)