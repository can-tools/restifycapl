---
meta-msapplication-config: ../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: String Literal
---

[Open topic with navigation](../../../../CANoe.htm#Topics/CAPLFunctions/Other/CAPLfunctionsStringLiteral.htm)

[CAPL Functions](../CAPLfunctions.htm) Â» [General](CAPLGeneralStartPage.htm) Â» String Literal

# String Literal

 

[Valid for](../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

A string literal is a character string in quotation marks: "This is a character string"

| ![Note](../../../Resources/vImages/vInfo.png "Note") | Note<br>Like in the C language, when storing string literals in char arrays, one char at the end of the array needs to be reserved for the string termination character (/0), i.e. the char array needs to be one element longer than the pure string length. |
| ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Escape Sequences

Certain characters are displayed as a combination of characters with a preceding backslash (escape sequence) within a character string, e.g:

| Description           | Display Inside Character Strings |
| --------------------- | -------------------------------- |
| New line              | \n                               |
| Tabulator             | \t                               |
| Backslash             | \\                               |
| Carriage return       | \r                               |
| Backspace             | \b                               |
| Double quotation mark | \"                               |
| Single quotation mark | \'                               |

| ![Note](../../../Resources/vImages/vInfo.png "Note") | Note<br>The CAPL string encoding usually matches the encoding of the source file. When you create a file with the CAPL Browser, it will save the file in an encoding which matches the language settings of your computer and write the encoding in a special comment at the beginning of the file. If the source file is encoded in UTF-16 or if an include file has a different encoding than the source file, the CAPL string encoding will be UTF-8. |
| ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../Shared/HowToUseOnlineHelp.htm](../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)