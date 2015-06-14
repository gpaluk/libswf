#include <assert.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <zlib.h>

#include "SwfParser.h"
#include "FileAttributesTag.h"
#include "SetBackgroundColorTag.h"
#include "DefineSceneAndFrameLabelDataTag.h"
#include "DefineFont3Tag.h"
#include "EndTag.h"


SwfParser::SwfParser() {
    swf = new Swf();
}

SwfParser::SwfParser(Swf *swf) {
    this->swf = swf;
}

SwfParser::~SwfParser() {
    delete swf;
    delete ds;
}

void SwfParser::readFromFile(const char *fileName)
{
    if (fileName) {
        ifstream inputFile (fileName, ios::binary);

        inputFile.seekg (0, inputFile.end);
        int length = inputFile.tellg();
        inputFile.seekg (0, inputFile.beg);

        cout << length << endl;

        uint8_t data[length];
        inputFile.read((char*)&data, length);

        inputFile.close();

        readFromRawData(data, length);
    }
}

void SwfParser::readFromRawData(uint8_t *data, size_t dataLength)
{
    if (dataLength < 8) {
        return;
    }

    // Create buffer for header and copy header
    uint8_t header [8];
    copy(data, data + 8, header);
    ds = new DataStream(header, sizeof(header));

    swf->compression = ds->readCompression();
    swf->version = ds->readUI8();
    swf->fileLength = ds->readUI32();

    if (swf->compression == SwfCompression::ZLIB) {
        unsigned long uncompressedDataSize = (swf->fileLength - 8);
        unsigned long compressedDataSize = (dataLength - 8);

        uint8_t body [uncompressedDataSize];
        uncompress(body, &uncompressedDataSize, data + 8, compressedDataSize);
        ds = new DataStream(body, sizeof(body));
    }

    swf->frameSize = ds->readRect();
    ds->readUI8();  // Skip first byte of frameRate because this looks like big-endian instead little
    swf->frameRate = ds->readUI8();
    swf->frameCount = ds->readUI16();

    readTagList();
}



Tag SwfParser::readTag() {
    uint16_t tagIdAndLength = ds->readUI16();
	uint16_t tagId = tagIdAndLength >> 6;  // Upper 10 bits: tag ID
	uint32_t tagLength = tagIdAndLength & 0x3F;
    if (ds->available() < tagLength)
        return Tag();

	cout << "Tag ID is: " << (int)tagId << endl;

	if (tagLength == 0x3F) {
        tagLength = ds->readUI32();
        if (ds->available() < tagLength)
            return Tag();
	}
	cout << "Tag length is: " << (int)tagLength << endl;

    if (tagId == 0)
        return EndTag(ds);
    else if (tagId == 9)
        return SetBackgroundColorTag(ds);
	else if (tagId == 69)
        return FileAttributesTag(ds);
	else if (tagId == 75)
        return DefineFont3Tag(ds);
	else if (tagId == 86)
        return DefineSceneAndFrameLabelDataTag(ds);
    else
        ds->skipBytes(tagLength);

    return Tag();
}

void SwfParser::readTagList() {
    vector<Tag> tagList;
    while (ds->available() > 0) {
        Tag t = readTag();
        if (t.id)
            tagList.insert(tagList.end(), t);
        if (t.id == 0)
            break;
	}
}