---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Parameter Check in CAPL Functions
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/FunctionsParameterCheck.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [CAPL Functions](Functions.htm) Â» Parameter Check in CAPL Functions

# Parameter Check in CAPL Functions

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

A parameter check is made like in C++.

The type of the passed argument must correspond to the type of the parameter, thus e.g.:

| Argument Type | Parameter Type                                |
| ------------- | --------------------------------------------- |
| Numeric       | Numeric (byte, char, ... qword, int64, float) |
| CAN message   | CAN message (other message types accordingly) |
| msTimer       | msTimer (other timer types accordingly)       |

## Numeric Types

For scalar numeric types, an implicit cast (type conversion) is performed.

If overloaded functions contain a parameter with numeric type in each case, an explicit cast is required if the passed argument does not correspond exactly to a parameter type.

void Function1(long lpar) {}  
void Function1(dword lpar) {}  
void Function2(dword lpar) {}  
void Function3()  
{  
byte bvar;  
long lvar;  
Function1((long)bvar); // explicit cast  
Function1((dword)bvar); // explicit cast  
Function1(lvar); // no cast  
Function2(bvar); // implicit cast  
}

For numeric array types, e.g., byte par[], the passed parameter must also be a byte array.

## Message Types

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE,

If a message parameter with a wildcard is declared, e.g., for CAN messages message \* msgPar, all CAN messages can then be passed.

If a message parameter with a concrete message or ID is declared, e.g., message CarSpeed msgPar, the passed parameter must also have this same type.

[Glossary](../../../Shared/Glossary.htm) â¢ [Event Procedures](../../../CAPLFunctions/Other/CAPLfunctionsEventProceduresOverview.htm) â¢ [Keywords](Keywords.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)