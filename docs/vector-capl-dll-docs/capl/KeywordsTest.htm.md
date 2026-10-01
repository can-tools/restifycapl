---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Keywords: Test
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/KeywordsTest.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [Keywords](Keywords.htm) Â» Test

# Keywords: Test

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

| Keyword      | Description                                                                                                                                                                                              |
| ------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| testcase     | Defines a test case. A [Â» test case](../../../Shared/Glossary.htm#TestCase) is the semantic test entity. It returns a unique test result in form of a [Â» verdict](../../../Shared/Glossary.htm#Verdict) and appears in the [Â» test report](../../../Shared/Glossary.htm#TestReport).  <br>Example: <br>testcase MyTestCase()  {  }<br>[CAPL Test Module](../../../CANoeCANalyzer/AutomatedTesting/TestModules/CAPLTestModuleStructure.htm) \| [.NET Test Module](../../../CANoeCANalyzer/AutomatedTesting/TestModules/NETTestModuleCallCAPLTestCaseFunction.htm) \| [XML File](../../../CANoeCANalyzer/AutomatedTesting/TestModules/XMLFileCallCAPLTestCase.htm) |
| testfunction | Defines a test function. A test function is a function, that can be (re-)used in [Â» test cases](../../../Shared/Glossary.htm#TestCase) and [Â» test sequences](../../../Shared/Glossary.htm#TestSequence). In difference to other functions a test function is reported as a block in the [Â» test report](../../../Shared/Glossary.htm#TestReport) automatically.  <br>Example: <br>testfunction MyTestFunction()   {   }<br>[.NET Test Module](../../../CANoeCANalyzer/AutomatedTesting/TestModules/NETTestModuleCallCAPLTestCaseFunction.htm) \| [XML File](../../../CANoeCANalyzer/AutomatedTesting/TestModules/XMLFileCallCAPLTestFunction.htm) |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>CAPL test functions do not provide a return value. |
| ------------------------------------------------------- | ---------------------------------------------------------- |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)