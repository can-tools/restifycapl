---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: CAPL DLL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/CAPLDLL/CAPLIncludeWindowsDLL.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» CAPL DLL

# CAPL DLL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

In CAPL programs you can call functions which you have implemented in your own C/C++ DLL. In doing so, the function from the DLL are exported through a function table.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note for CANoe DE,<br>A 64-bit DLL is required for the Measurement Setup.<br>The sample project also contains the code for this case. |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |

The CAPL DLL interface also supports callback functions.

* [Description of the CAPL Export Table](CAPLExportTable.htm)
* [Example of a C++ DLL for CAPL](CAPLIncludeWindowsDLLExample.htm)
* [Search sequence for a Windows DLL](CAPLIncludeWindowsDLLSearchSequence.htm)

Whenever functions of a DLL are called in the realtime area ([Simulation Setup](../../../CANoeCANalyzer/Windows/SimulationSetup/SimulationSetupWindow.htm)), they run in a high-priority thread. This can affect measurement.

In order for the CAPL compiler and CAPL Browser to recognize the DLL, you must link it to the CAPL program.

To do this, proceed as follows:

* Enter the DLL in the [Options](../../../CANoeCANalyzer/Ribbon/File/Options/Programming/ProgrammingCAPLDLL.htm) dialog.  
In this case, the DLL will be available to all CAPL programs.
* You can enter the DLL in the [includes section](../IncludeFiles/IncludeFiles.htm) of a CAPL program using the [#pragma library](../General/CompilerCommandsPragmaLibrary.htm) command.  
In this case, it will only be available to this program.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>When linking your own DLLs in CAPL programs, all methods are available to you of accessing system resources (Memory management, hardware, etc). However, only experienced programmers should make use of this option due to the multitude of associated problems. We ask for your understanding that Vector is not able to provide any support for creating DLLs. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[CAPL DLL Sample Configuration](../../../SampConf/Programming/CAPLDLL/CAPLDLLOverview.htm) â¢ [Switches: CAPL DLL API](../../../HardwareInterfaces/MatrixSwitches/MatrixSwitchesCAPLDLLAPI.htm) â¢ [Assigning of Modeling Libraries](../../../CANoeCANalyzer/LibrariesPackages/AddOnDLLsAssigning.htm) â¢ [Overview - Modeling Libraries / Add-ons](../../../CANoeCANalyzer/LibrariesPackages/OEMPackages.htm) â¢ [.NET DLL](../NETDLL/CAPLIncludeNETDLL.htm) 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)