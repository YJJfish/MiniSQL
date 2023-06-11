#pragma once
#include <iostream>
#include "Global.h"
#include "BufferManager.h"
using namespace std;

class CatalogManager {
public:
	CatalogManager(void) {}
	~CatalogManager(void) {};

	bool TableExists(const string& TableName);

	void CreateTable(const TableSchema& table);

	void CreateIndex(const string& TableName,const IndexSchema& index);

	void DropTable(const string& TableName);

	bool IndexExists(const string& IndexName);

	void DropIndex(const string& IndexName);

	void GetTable(const string& TableName, TableSchema& table);

	bool GetIndex(const string& TableName, const string& AttrName, IndexSchema& index);

};