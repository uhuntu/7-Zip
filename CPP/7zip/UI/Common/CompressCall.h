// CompressCall.h

#ifndef ZIP7_INC_COMPRESS_CALL_H
#define ZIP7_INC_COMPRESS_CALL_H

#include "../../../Common/MyString.h"

UString GetQuotedString(const UString &s);

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish);

void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
void TestArchives(const UStringVector &arcPaths, bool hashMode = false);
FString GuidArchives(UStringVector& arcPaths, bool hashMode = false);

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName);

void Benchmark(bool totalMode);

#endif
