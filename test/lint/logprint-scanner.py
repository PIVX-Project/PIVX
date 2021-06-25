#!/usr/bin/env python3
# Copyright (c) 2017-2018 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os, sys
from subprocess import check_output

def countRelevantCommas(line):
    openParensPosStack = []
    openParensPos = 0
    charCounter = 0
    numRelevantCommas = 0
    firstOpenParensIndex = line.find("(")

    for char in line:
        if char == '(':
            openParensPosStack.append(charCounter)

        if char == ')':
            x = openParensPosStack.pop()
            if x == firstOpenParensIndex:
                return numRelevantCommas

        if char == "," and openParensPosStack[-1] == firstOpenParensIndex:
            numRelevantCommas += 1
        charCounter += 1

    return numRelevantCommas

if __name__ == "__main__":
    out = check_output("git rev-parse --show-toplevel", shell=True, universal_newlines=True)
    srcDir = out.rstrip() + "/src/"

    filelist = [os.path.join(dp, f) for dp, dn, filenames in os.walk(srcDir) for f in filenames if os.path.splitext(f)[1] == '.cpp' or os.path.splitext(f)[1] == '.h' ]
    incorrectInstanceCounter = 0

    for file in filelist:
        f = open(file,"r", encoding="utf-8")
        data = f.read()
        rows = data.split("\n")
        count = 0
        full_data = []
        lineCounter = 1

        tempLine = ""
        tempCount = 0

        for row in rows:
            # Collapse multiple lines into one
            tempLine += row

            # Line contains LogPrint, LogPrintf or error
            if tempLine.find("LogPrint") != -1 or \
                    (tempLine.find("error(") != -1 and tempLine.find("_error") == -1)  or \
                    tempLine.find("strprintf") != -1:
                if tempLine.count("(") == tempLine.count(")"):
                    havePercents = tempLine.count('%') > 0

                    if havePercents:
                        # This line of code has a format specifier that requires checking number of associated arguments
                        # Determine the number of arguments provided, see if that matches the number of format specifiers
                        # Count the number of commas after the format specifier string.  Check to see if it matches the number of format specifiers.
                        # Assumes quotes are not escaped in the specifier string and there are no percent signs when specifying the debug level.

                        # First, determine the position of the comma after the format specifier section, named commaAfterEndSpecifierStringIndex
                        firstSpecifierIndex = tempLine.find('%')
                        endSpecifierStringIndex = tempLine.find('"',firstSpecifierIndex)

                        # Skip escaped quotes
                        while tempLine[endSpecifierStringIndex - 1] == '\\':
                            endSpecifierStringIndex = tempLine.find('"', endSpecifierStringIndex + 1)

                        commaAfterEndSpecifierStringIndex = tempLine.find(',',endSpecifierStringIndex)

                        # Count the number of commas after the specifier string
                        line = "(" + tempLine[commaAfterEndSpecifierStringIndex:-1]
                        numCommas = countRelevantCommas(line)

                        # Determine number of extra percents after specifier string
                        numExtraPercents = tempLine.count('%', commaAfterEndSpecifierStringIndex)

                        # Subtract extra from total count.  This is the number of expected specifiers
                        # ignore %%
                        numPercents = tempLine.count('%') - numExtraPercents - 2*tempLine.count('%%')

                        if numPercents != numCommas:
                            print("Incorrect number of arguments for LogPrint(f)/error/strprintf statement found.")
                            print(str(file) + ":" + str(lineCounter - tempCount))
                            print("Line = " + tempLine)
                            print("numRelevantCommas = " + str(numCommas) + ", numRelevantPercents = " + str(numPercents))
                            print("")

                            incorrectInstanceCounter += 1

                    # Done with this multiline, clear tempLine
                    tempLine = ""
                    tempCount = 0
                else:
                    tempCount += 1
            else:
                # No LogPrint, clear tempLine
                tempLine = ""
                tempCount = 0

            lineCounter += 1

    print("# of incorrect instances: " + str(incorrectInstanceCounter))

    sys.exit(incorrectInstanceCounter)
