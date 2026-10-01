---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Instructions, Expressions, Operators in CAPL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/Expressions.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Instructions, Expressions, Operators in CAPL

# Instructions, Expressions, Operators in CAPL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

The CAPL syntax is based on the programming language C. The following instructions, expressions and operators are permitted as in C:

## [ClosedInstructions](javascript:void(0))

The following instructions are [keywords](Keywords.htm).

* Instruction blocks: { ... }

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>{    temp = msg.sig\_1;    msg.sig\_1 = msg.sig\_2;    msg.sig\_2 = temp;   } |
    | ---------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |

* if { ... } and if {...} else { ... }

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>if (count < 50) count++;   ...   if (x < y) min = x; else min = y; |
    | ---------------------------------------------------------------- | ----------------------------------------------------------------------------- |

* switch, case, default

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>switch (component) {    case(1) :    @comp\_1 = value; break;    case(2) :    @comp\_2 = value; break;    default :    write("error: wrong parameter (%d)",component); stop();    break;   } |
    | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

* for.., while.., do..while loops

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>for (i = 0; i < 100; i++) sum += array[i];   ...   while(pos < msg.DLC) {    sum\_even += msg.byte(pos++);    sum\_odd += msg.byte(pos++);   }   ...   do {    sum = sum += array[i];   } while (i < 100 && sum <= 1000); |
    | ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

* continue and break

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>for (i = 0; i < 100; i++) {    if (array[i] == 0) continue;    array[i] = 1/array[i];   }   ...   all\_valid = 1;   len = elCount(is\_valid);   for (i = 0; i < len; i++) {    if (is\_valid[i] == 0) {    all\_valid = 0;     break;    }   }   if (all\_valid) ... |
    | ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

* return

    | ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>double sqr(double x) {    return(x \* x);   } |
    | ---------------------------------------------------------------- | -------------------------------------------------------- |

## [ClosedExpressions](javascript:void(0))

In evaluating expressions containing LONG, DWORD, INT, WORD, CHAR and BYTE, an implicit type conversion is performed. The following rules apply:

* 8 bit variables are first converted to 32 bit (CHAR -> LONG, BYTE -> DWORD)
* 16 bit variables are first converted to 32 bit (INT -> LONG, WORD -> DWORD)

If an expression contains a 64 bit operand, the other operand can be converted to 64 bit (if necessary).

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example "?"<br>min = x < y ? x : y; |
| ---------------------------------------------------------------- | ----------------------------------- |

The float type can be implicitly converted to an integer type. This conversion is saturating, i.e. values that lie outside the integer range are mapped to the largest or smallest possible value. An exception to this rule is the conversion to integers with less than 32 bits. For these types, a saturating conversion is first carried out to the corresponding signed 32-bit wide type and the resulting value is then truncated.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>In versions prior to 17.3, implicit conversions that were applied at compile time (e.g. when initializing a variable or constant) were not saturated.<br>If such conversions exist in the CAPL code, the CAPL compiler issues a warning as of version 17.3.<br>This only affects integer values that have been initialized with a float constant.<br>The changed conversion (from version 17.3) may have an effect on the executed code because the value in the variable or constant is then different. However, this is only rarely the case on the Windows operating system.<br>An example would be the following initialization:<br>const dword d = 10000000000000000000.0;<br>With the old behavior you get 0 as the result (under Windows); with the new behavior you get 4294967295.<br>To restore the old behavior, you can set the switch LegacyNumericInitializations in the file <configuration directory>/Exec64/ConfigFileSwitches.cfg.ini to 1. This file is created when a configuration is saved from version 17.3.<br>Only set the switch to 1 if you have an old CAPL program which now issues a warning and you cannot change the CAPL program (i.e. the old (actually incorrect) value is used).<br>We recommend adapting the CAPL program, i.e. simply using an integer constant instead of the float constant. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedOperators](javascript:void(0))

In CAPL, the same arithmetic, logical and bitwise operators are provided to you ( +,\*, +=, \*=, ||, ++, etc.) as in C.

For a bitwise comparison the ^ operator is available.

At equality the bit will be set to 0, otherwise to 1.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Result = (value1 ^ value2) |
| ---------------------------------------------------------------- | ------------------------------------- |

If you want to negate all bits, you have to use the ~ operator additionally.

With this all bits with value 1 will be set to 0 an vice versa.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Result = ~(value1 ^ value2) |
| ---------------------------------------------------------------- | -------------------------------------- |

You can perform a type conversion explicitly with the Cast operator (as in C). A Cast with entry of a message denotation can be used to assign different identifiers to messages.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example for Message-Denotation<br>on message 0x100<br>{    message 0x101 msg;    msg = (message 0x101) this;    output (msg); //message with id 0x101 will send to bus   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Logical Operators

| Symbol | Meaning               |
| ------ | --------------------- |
| ==     | equal                 |
| !=     | not equal             |
| <      | less than             |
| <=     | less than or equal    |
| >      | greater than          |
| >=     | greater than or equal |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note to Comparison<br>A signed comparison is executed if one operand is of data type signed and one of data type unsigned.<br>An unsigned comparison is only executed if both operands are of data type unsigned. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Write Format Expressions](../../../CAPLFunctions/Other/CAPLFunctionsWriteFormatExpressions.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)