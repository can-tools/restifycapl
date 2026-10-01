---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: memcmp
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCmp.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [Struct Byte Access](../CAPLfunctionsStructByteAccessOverview.htm) Â» memcmp

# memcmp

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Function Syntax

int memcmp(struct \* dest, byte source[]); // form 1

int memcmp(byte dest[], struct \* source); // form 2

int memcmp(struct \* dest, struct \* source); // form 3

int memcmp(byte dest[], byte source[], dword size); // form 4

int memcmp(struct dest, struct source); // form 5

int memcmp(array dest, array source); // form 6

int memcmp(union dest, union source); // form 7

int memcmp(struct dest, byte source[]); // form 8

int memcmp(array dest, byte source[]); // form 9

int memcmp(union dest, byte source[]); // form 10

int memcmp(byte dest[], struct source); // form 11

int memcmp(byte dest[], array source); // form 12

int memcmp(byte dest[], union source); // form 13

## Description

Compares the bytes of the parameters.

In form 3, both structs must have the same type.

Forms 5-13 compare vCDL objects with themselves or byte arrays.

In form 5-7, dest and source must habe the same type.

In form 8-13, the vCDL object must have a fixed layout.

## Parameters

| dest   | A struct / byte array / vCDL object              |
| ------ | ------------------------------------------------ |
| source | Another struct / byte array / vCDL object        |
| size   | Size of the arrays (number of bytes to compare). |

## Return Values

0 if the bytes are equal; a value different from 0 if they are unequal

## Example

| ![Example](../../../../Resources/vImages/vExample.png "Example") | byte data[4];   struct WrapDword   {    dword dw;   } dwordWrapper;    int i;   for (i = 0; i < elcount(data); ++i)    data[i] = i;   dwordWrapper.dw = 0x03020100;   if (memcmp(dwordWrapper, data) == 0)   write("Data represents the number: Little Endian is used."); |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Availability

## [ClosedCANalyzer](javascript:void(0))

|                                     | CANalyzer                             |
| ----------------------------------- | ------------------------------------- |
| Since Version                       | < 12.0: form 1-4<br>17 SP2: form 5-13 |
| Restricted To                       | â                                   |
| Measurement Setup (Transmit Branch) | â                                   |
| Measurement Setup (Analysis Branch) | â                                   |
| 32-Bit                              | â                                   |
| 64-Bit                              | â                                   |

## [ClosedCANoe Desktop Editions](javascript:void(0))

|                             | CANoe DE                              | CANoe MedTech DE | CANoe4SW:lite DE                  |
| --------------------------- | ------------------------------------- | ---------------- | --------------------------------- |
| Since Version               | < 12.0: form 1-4<br>17 SP2: form 5-13 | 18: form 1-13    | 14: form 1-4<br>17 SP2: form 5-13 |
| Restricted To               | â                                   | â              | â                               |
| Measurement Setup           | â                                   | N/A              | N/A                               |
| Simulation Setup            | â                                   | â              | â                               |
| Communication Setup         | â                                   | â              | â                               |
| Test Setup for Test Modules | â                                   | â              | N/A                               |
| Test Setup for Test Units   | â                                   | â              | â                               |
| 32-Bit                      | â                                   | â              | â                               |
| 64-Bit                      | â                                   | â              | â                               |

## [ClosedCANoe Server Editions](javascript:void(0))

|                    | CANoe SE   (Windows) | CANoe SE   (Linux) | CANoe4SW SE   (Windows)             | CANoe4SW SE   (Linux)               | CANoe MedTech SE   (Windows) | CANoe MedTech SE   (Linux) |
| ------------------ | -------------------- | ------------------ | ----------------------------------- | ----------------------------------- | ---------------------------- | -------------------------- |
| Since Version      | 18: form 1-13        | 18: form 1-13      | 13.0: form 1-4<br>17 SP2: form 5-13 | 13.0: form 1-4<br>17 SP2: form 5-13 | 19: form 1-13                | 19: form 1-13              |
| Restricted To      | â                  | â                | â                                 | â                                 | â                          | â                        |
| Simulation Nodes   | â                  | â                | â                                 | â                                 | â                          | â                        |
| Application Models | â                  | â                | â                                 | â                                 | â                          | â                        |
| Test Units         | â                  | â                | â                                 | â                                 | â                          | â                        |
| 32-Bit             | â                  | N/A                | â                                 | N/A                                 | â                          | N/A                        |
| 64-Bit             | â                  | â                | â                                 | â                                 | â                          | â                        |

## [ClosedVector Test Unit Runner](javascript:void(0))

|               | Vector Test Unit Runner |
| ------------- | ----------------------- |
| Since Version | 17                      |

## [ClosedvTESTstudio](javascript:void(0))

|               | vTESTstudio                         |
| ------------- | ----------------------------------- |
| Since Version | < 4.0: form 1-4<br>8 SP2: form 5-13 |
| Restricted To | â                                 |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)