---
meta-msapplication-config: ../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Compile CAPL Programs
---

[Open topic with navigation](../../../../CANoe.htm#Topics/CAPLBrowser/General/CAPLBrowserCompileCAPLPrograms.htm)

[![](../../../Resources/vImages/vToolIcons/CANoeDEFamily_Icon16_15x15.png) CANoe](../../CANoeCANalyzer/CANoe.htm) Â» [CAPL Introduction](../../ProgrammingInterfaces/CAPL/CAPLIntroduction.htm) Â» Compile CAPL Programs

# Compile CAPL Programs

 

[Valid for](../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

In order to create an executable program file in CBF format (CAPL Binary Format, filename extension \*.cbf), you must compile the program with the CAPL Compiler.

A successfully compiled file will then be saved automatically insofar as this is set accordingly in the [editor options](../Ribbon/CBRibbonFileOptions.htm).

Compiling of a program is started by the Compile command by clicking ![](../../../Resources/vImages/vIcons/compile_16.png) in the [Home ribbon tab](../Ribbon/CBRibbonHome.htm) or by pressing the <F9> key. If the CAPL program does not have a filename yet, you are asked to enter a name for the CBF file. If the program already has a name, the name of the CBF file is composed of the program name and the extension .cbf.

Any errors or warnings that occur during the compiling process are listed in the [Output](../../Shared/OutputWindow.htm) view.

| ![Note](../../../Resources/vImages/vInfo.png "Note") | Note<br>In the CAPL code outside of quotation marks (""), no special characters are allowed. This concerns, among others, especially the German "Umlaut" charactersspace characters of some local code pages with an encoding of more than one byte<br>Special characters are not tolerated by the CAPL Compiler, but result in a compile error. |
| ---------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../Shared/HowToUseOnlineHelp.htm](../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)