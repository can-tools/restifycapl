---
meta-msapplication-config: ../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Reuse CAPL Code
---

[Open topic with navigation](../../../../CANoe.htm#Topics/CAPLBrowser/General/CAPLBrowserMacrosReuseCAPLCode.htm)

[![](../../../Resources/vImages/vToolIcons/CANoeDEFamily_Icon16_15x15.png) CANoe](../../CANoeCANalyzer/CANoe.htm) Â» [CAPL Introduction](../../ProgrammingInterfaces/CAPL/CAPLIntroduction.htm) Â» Macros Â» Reuse CAPL Code

# Reuse CAPL Code

 

[Valid for](../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

With the help of special macros, you can reuse CAPL code more easily in multiple Simulation Setup nodes. The macros are replaced with text by the CAPL Compiler prior to subsequent processing in the program.

The following macros are available:

| Macro                       | Short Description                                                                                                                                                                                 |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| %BASE\_FILE\_NAME%          | Name of the .can file that is just compiled (e.g. SomeFile.can); especially useful in [include files](../../ProgrammingInterfaces/CAPL/IncludeFiles/IncludeFiles.htm).                            |
| %BASE\_FILE\_NAME\_NO\_EXT% | Name of the .can file that is just compiled but without filename extension (e.g. SomeFile); especially useful in [include files](../../ProgrammingInterfaces/CAPL/IncludeFiles/IncludeFiles.htm). |
| %BUS\_TYPE%                 | Bus system of the channel to which the node is assigned.                                                                                                                                          |
| %CHANNEL%                   | Number of the channel to which the node is assigned.                                                                                                                                              |
| %FILE\_NAME%                | Name of the file that contains the source code (e.g. SomeIncludeFile.cin).                                                                                                                        |
| %FILE\_NAME\_NO\_EXT%       | Name of the file that contains the source code but without filename extension (e.g. SomeIncludeFile).                                                                                             |
| %LINE\_NUMBER%              | Number of the line in the file containing the macro.                                                                                                                                              |
| %NETWORK\_NAME%             | Name of the network to which the node is assigned.                                                                                                                                                |
| %NODE\_NAME%                | Name of the node.                                                                                                                                                                                 |

| ![Example](../../../Resources/vImages/vExample.png "Example") | Example<br>write("The node name is %NODE\_NAME%");   sysSetVariableString(sysvar::MySpace::VarString, "%CHANNEL%");   write("Activated functionality in network %NETWORK\_NAME% (Channel %BUS\_TYPE%%CHANNEL%)"); |
| ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Note](../../../Resources/vImages/vInfo.png "Note") | Note<br>The macros %NODE\_NAME%, %BUS\_TYPE%, %CHANNEL% and %NETWORK\_NAME% are only useful in the Simulation Setup; nevertheless they are allowed at other places, too.The macros %BUS\_TYPE%, %CHANNEL% and %NETWORK\_NAME% in gateway nodes characterize the first bus system / channel and the first network/node, respectively. |
| ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../Shared/HowToUseOnlineHelp.htm](../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)