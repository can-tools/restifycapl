---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Symbolic Database Access
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/SymbolicAccessDatabase.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Symbolic Database Access

# Symbolic Database Access

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

CAPL enables symbolic access to database objects with use in modeling libraries (DLL files), e.g. TestServiceLibrary.

Database objects are identified with the following syntax

* Network access:  
[dbNetwork::]DBName
* Node access:  
[[dbNetwork::]DBName::][dbNode::]NodeName
* Message access:  
[[dbNetwork::]DBName::][[dbNode::]NodeName::][dbMsg::]MessageName
* Signal access:  
[[dbNetwork::]DBName::][[dbNode::]NodeName::][[dbMsg::]MessageName::][dbSig::]SignalName

The specification of the message name is required if a signal is assigned to a particular message .

The names used for the identification of an object are each separated by the qualifiers "::".

The keywords

* dbNetwork
* dbNode
* dbMsg
* dbSig

are optional and are only required if e.g. a node and a message have the same name.

Instead of a node name, you can also use the keyword thisNode. Here is meant the node name that was assigned to the node in the configuration dialog for network nodes.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Use of Test Service Library:<br>mSigCheckId = ChkStart\_MsgSignalValueRangeViolation(   MsgValue::SigValue, // Signal to supervise   lSigMinValue, // Minimum allowed value   lSigMaxValue, // Maximum allowed value   "SigCallback"); // CAPL callback for violation notification |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)