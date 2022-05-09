#!/usr/bin/env python3.9

import functools
import os

cerr = print = functools.partial(print, flush=True)

def getFilePaths(dirPath, fileNameEnds=set(), excludedFileNameEnds=set()):
    assert isinstance(fileNameEnds, set)
    assert isinstance(excludedFileNameEnds, set)
    dirPath = dirPath.rstrip('/')
    filePaths = []
    for (subdirPath, _, fileNames) in os.walk(os.path.expanduser(dirPath)):
        for fileName in fileNames:
            if not fileNameEnds or any([fileName.endswith(fileNameEnd) for fileNameEnd in fileNameEnds]):
                if all([not fileName.endswith(suffix) for suffix in excludedFileNameEnds]):
                    filePath = os.path.join(subdirPath, fileName)
                    filePaths.append(filePath)
    return sorted(filePaths)

def justifyInt(num, digits):
    return str(num).rjust(digits, '0')

def truncateFloat(num, decimalPlaces):
    return float(f'{num:.{decimalPlaces}f}')
