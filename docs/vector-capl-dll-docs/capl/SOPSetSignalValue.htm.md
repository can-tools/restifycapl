---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Setting Signal Values
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/SignalOrientedProgramming/SOPSetSignalValue.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [Concept of a Signal in CAPL](SOPSignalConcept.htm) Â» Setting Signal Values

# Setting Signal Values

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

In CAPL, you can directly set signal values.

For this purpose, write a dollar sign $ before the signal name.

$EngineSpeed = 500.0;

However, this requires that the configuration features a suitable [Interaction Layer](../../../CANoeCANalyzer/LibrariesPackages/VectorILCAN/VectorILCAN.htm).

For the access to the raw value or explicitly to the physical value of a signal, you can use .raw, .raw64 or .phys for setting signal values in the same way as for [reading signal values](SOPReadSignalValue.htm).

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>The signal value does not change immediately, only after the signal is being transmitted again on the network.<br>The syntax always reads out the last value transmitted to the network.<br>To explicitly read out the last value transmitted to the network, you can also attach .rx to the signal name. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

For this reason, the program

$EngineSpeed = 500.0;  
if ($EngineSpeed != 500.0) write("Unequal!");

usually outputs "Unequal!" in the Write Window. If you want to read out a value that has been changed in the program, but not yet transmitted on the network, you can use the suffix .txrq:

if ($EngineSpeed.txrq != 500.0)

However, this is allowed only in programs in Simulation Setup. .txrq can also be combined with .raw, .raw64 or .phys:

$EngineSpeed.txrq.raw

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Saving the signal values is always discrete. If a physical value is assigned, the next closest discrete raw value is entered after scaling. If the signal is subsequently read out, the result is not necessarily the original value. The deviation for floating point numbers is usually negligible.The access to the txRq property does not work for [service signals](../../../CANoeCANalyzer/Ethernet/ILSomeIP/ILSomeIPServiceSignals.htm). |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[![Concept Link Icon](../../../../../Skins/Default/Stylesheets/Images/transparent.gif)See Also](javascript:void(0);)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)