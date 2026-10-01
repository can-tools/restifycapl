---
meta-msapplication-config: ../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Write Format Expressions
---

[Open topic with navigation](../../../../CANoe.htm#Topics/CAPLFunctions/Other/CAPLFunctionsWriteFormatExpressions.htm)

[CAPL Functions](../CAPLfunctions.htm) Â» [General](CAPLGeneralStartPage.htm) Â» Write Format Expressions

# Write Format Expressions

 

[Valid for](../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

Formatting takes place via placeholders within the format string. For example, if a program wanted to print out the current simulation time to the [Write Window](../../CANoeCANalyzer/Windows/Write/WriteWindow.htm), it could present the output by prefixing it with The current simulation time is, and using the format specifier %lld to denote that it wants the number of nanoseconds for the simulation time to be shown immediately after that message, it may use the format string:

Write("The current simulation time is %lld ns", timeNowInt64());

## Comparison to C / C++

The placeholder format is very similar to the one used in the C or C++ languages. In almost all cases, you can use the format known to you if you have experience in one of those languages. Some differences include:

* The flag for thousands separators ("'") is not supported.
* The types "a" and "A" are not supported.
* The type "n" is not supported.
* The exponent with types "e" and "E" always contains three digits on windows.

## Syntax

The syntax for a format placeholder is

%[flags][width][.precision][length]type

If a % sign shall be printed, it must be masked by another % sign: "%%".

## Flags Field

The Flags field can be zero or more (in any order) of:

| Character | Description                                                                                                                                                                                              |
| --------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| - (minus) | Left-align the output of this placeholder. (The default is to right-align the output.)                                                                                                                   |
| + (plus)  | Prepends a plus for positive signed-numeric types. positive = +, negative = -.<br>The default does not prepend anything in front of positive numbers.                                                    |
| (space)   | Prepends a space for positive signed-numeric types. positive = , negative = -. This flag is ignored if the + flag exists.<br>The default does not prepend anything in front of positive numbers.         |
| 0 (zero)  | When the 'width' option is specified, prepends zeros for numeric types.<br>The default prepends spaces.<br>For example, Write("%4X",3) produces  3, while Write("%04X",3) produces 0003.                 |
| # (hash)  | Alternate form:<br>For g and G types, trailing zeros are not removed.<br>For f, F, e, E, g, G types, the output always contains a decimal point.<br>For o, x, X, b, B types, the text 0, 0x, 0X, 0b, 0B respectively, is prepended to non-zero numbers. |

## Width Field

The width field specifies a minimum number of characters to output and is typically used to pad fixed-width fields in tabulated output, where the fields would otherwise be smaller, although it does not cause truncation of oversized fields.

The width field may be omitted, or a numeric integer value, or a dynamic value when passed as another argument when indicated by an asterisk \*. For example, Write("%\*d", 5, 10) will result in 10 being printed, with a total width of 5 characters.

Though not part of the width field, a leading zero is interpreted as the zero-padding flag mentioned above, and a negative value is treated as the positive value in conjunction with the left-alignment - flag also mentioned above.

## Precision Field

The precision field usually specifies a maximum limit on the output, depending on the particular formatting type. For the e, E and f types, it specifies the number of digits to the right of the decimal point that the output should be rounded. For the g and G type, it specifies the number of all digits before and after the decimal point combined (but without the decimal point itself). For all floating point types, the default precision is 6 digits.

For the string type, it limits the number of characters that should be output, after which the string is truncated.

The precision field may be omitted, or a numeric integer value, or a dynamic value when passed as another argument when indicated by an asterisk \*. For example, Write("%.\*s", 3, "abcdef") will result in abc being printed.

## Length Field

The length field is needed for some of the integer types to tell the program how large the data type is. It is also sometimes different depending on whether the program runs on Windows or on Linux.

| CAPL Type | Size    | Length Field Windows | Length Field Linux |
| --------- | ------- | -------------------- | ------------------ |
| int       | 2 Bytes |                      |                    |
| long      | 4 Bytes | l                    |                    |
| int64     | 8 Bytes | I64 or ll            | l or ll            |
| byte      | 1 Byte  |                      |                    |
| word      | 2 Bytes |                      |                    |
| dword     | 4 Bytes | l                    |                    |
| qword     | 8 Bytes | I64 or ll            | l or ll            |

## Type Field

The type field depends on the data type of the expression that shall be printed, and can also designate different ways that the value is printed.

| CAPL Type                             | Type Description                                                                                                                                                                                         | Type   |
| ------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------ |
| int, long, int64                      | signed integer, decimal display                                                                                                                                                                          | d      |
| byte, word, dword, qword              | unsigned integer, decimal display                                                                                                                                                                        | u      |
| byte, word, long, dword, int64, qword | integer, hexadecimal display. Upper-case X in the type field means upper-case in the output.                                                                                                             | x or X |
| byte, word, long, dword, int64, qword | integer, octal display                                                                                                                                                                                   | o      |
| byte, word, long, dword, int64, qword | integer, binary display. Does not work on Windows. Only works on Linux with at least glibc version 2.35.<br>The case of the type field only has an effect with the # flag.                               | b or B |
| float, double                         | floating point, fixed point notation, e.g., 3.141593                                                                                                                                                     | f      |
| float, double                         | floating point, exponential notation, e.g., 3.141593e+000. The exponent will have 3 digits on Windows; it will have 2 or 3 digits on Linux.<br>Upper-case E in the type field means an upper-case E in the output. | e or E |
| float, double                         | floating point, use either fixed point or exponential notation, whichever is shorter                                                                                                                     | g or G |
| char                                  | character<br>When using a multibyte encoding like UTF8 (codepage 65001), the character format specifier may lead to undefined behavior if values beyond the ASCII range are used.                        | c      |
| char[]                                | string                                                                                                                                                                                                   | s      |

| ![Note](../../../Resources/vImages/vInfo.png "Note") | Note<br>The %n format is invalid and must not be used. |
| ---------------------------------------------------- | ------------------------------------------------------ |

 

- [../../Shared/HowToUseOnlineHelp.htm](../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)