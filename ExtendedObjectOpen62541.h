/*************************************************************************\
* Copyright (c) 2021 HZB.
* Author: Carsten Winkler carsten.winkler@helmholtz-berlin.de
\*************************************************************************/

#pragma once
#define EXO_USER_DICIONARIES_ONLY // optimised dictionary processing
#define ADDRESS_SIZE 8 // used for padding calculation
#define MEMORY_BANK_SIZE 4 // used for padding calculation
#define UA_NAMESPACEOFFSET 1000 // the name space offset is used to read data types with the same name space index from different dictionaries
#define EXO_DATANAME "Name"
#define EXO_DATATYPE "DataType"
#define EXO_DATATYPE_NS0ID "DataTypeNS0ID"


void binary_print(UA_Byte value);
UA_Int16 getDictionaries(UA_Client* client, std::map<UA_UInt32, std::string>* dictionaries);
UA_StatusCode getStructureValues(UA_Client* client, UA_Variant structVal, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, std::map<std::string, UA_Variant>* structureMembers);
UA_StatusCode getUserDataTypeAttributes(UA_Client* client, UA_NodeId pvId, std::map<UA_UInt32, std::string>* dictionaries, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes);
void printDictionaries(std::map<UA_UInt32, std::string>* dictionaries);
static UA_StatusCode UA_printEnum(const void* pData, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_String* output);
static UA_StatusCode UA_printStructure(const void* pData, std::map<std::string, UA_Variant>* structureMembers, UA_String* output);
static UA_StatusCode UA_printUnion(const void* pData, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_String* output);
