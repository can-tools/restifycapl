---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Search Sequence for a Windows DLL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/CAPLDLL/CAPLIncludeWindowsDLLSearchSequence.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [CAPL DLL](CAPLIncludeWindowsDLL.htm) Â» Search Sequence for a Windows DLL

# Search Sequence for a Windows DLL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

At program start DLLs are searched in the following sequence:

1. In the installation folder (e.g. c:\Programs\Vector CANoe).

* for CAPL DLLs in the simulation part with [32 bit execution environment](../../../CANoeCANalyzer/Ribbon/File/Options/Measurement/MeasurementGeneralSettings.htm) : subfolder Exec32.

* otherwise in the Exec64 subfolder.

2. In Windows system directory
3. In Windows directory
4. In the active working directory
5. In the folder of the path variables

For working with different user DLLs it is recommended to expand the path variable with the folder of the DLL before program start (e.g. with batch files from different subfolders).

Example of such a batch file:

@echo off  
set "CAPL_DLL_PATH=..\Libraries"  
set "PATH=%CAPL_DLL_PATH%;%PATH%"  
start MyConfiguration.cfg

Alternatively, the variants of the user DLLs can be placed in different working folders. Depending on the variant, the program must be started from the corresponding working folder. To do this, a shortcut must be created on the desktop or in the Start menu for each variant and the working folder containing the corresponding user DLL must be specified for each shortcut.

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)