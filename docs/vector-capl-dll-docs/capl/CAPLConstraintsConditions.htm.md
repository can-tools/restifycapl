---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Constraints and Conditions (CAPL Test Module)
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CANoeCANalyzer/AutomatedTesting/TestModules/CAPLConstraintsConditions.htm)

[Test Feature Set](../TestFeatures.htm) Â» [Constraints and conditions](../TestFeatureSet/TFSConstraintsConditions.htm) Â» CAPL

# Constraints and Conditions

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

Constraints and conditions are conditions, that are simultaneously being checked during the test sequence.

Any number of constraints and conditions for parallel monitoring may be added and they may be removed in any sequence from the active monitoring. This can occur for several test cases together. In the latter case, however, the monitoring is ended automatically with the end of the test case.

The state of the active constraints and conditions, that is whether or not these have already recognized an error, can be queried at any time in the test case with the help of the function [TestCheckConstraint](../../../CAPLFunctions/Test/Functions/CAPLfunctionTestCheckConstraint.htm)/[TestCheckCondition](../../../CAPLFunctions/Test/Functions/CAPLfunctionTestCheckCondition.htm). This way, it is possible to react purposefully to the failure of particular constraints/conditions should this be necessary sometime in exceptional situations.

Constraints and conditions can be defined easily with the functions (checks) of the [Test Service Library](../TestServiceLibrary.htm).

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>checkID = ChkStart\_MsgRelCycleTimeViolation (StatusMsg, 0.8, 1.2);   TestAddConstraint (checkID);   ..// cycle time of message StatusMsg will be checked   TestRemoveConstraint (checkID); |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |

Constraints and conditions can also monitor the occurrence of user-defined events, so called Test Events. Thereby a condition to be monitored can be programmed within an event procedure to send an according text event in case of an error.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>on message Status   {    if (this.error == 1)    TestSupplyTextEvent ("Error1");   }<br>testcase TC ()   {    TestAddConstraint ("Error1");    ...    TestRemoveConstraint ("Error1");   } |
| ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Test Feature Set: constraints and conditions](../TestFeatureSet/TFSConstraintsConditions.htm) â¢ [Test Service Library (TSL)](../TestServiceLibrary.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)