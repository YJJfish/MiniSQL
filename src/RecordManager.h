#pragma once
#include <iostream>
#include "Global.h"
#include "BufferManager.h"
using namespace std;

class RecordManager {
public:
	RecordManager(BufferManager& bm) :bm(bm) {};
	~RecordManager(void) {}

	void CreateTable(const TableSchema& table);

	void DropTable(const string& TableName);

	Pos* Intersect(Pos* table1, Pos* table2);

	int Select(const TableSchema& table, Pos* temptable,
			   int length, string AttrName[], int op[], void* values[],
			   const string& ResultFile);

	int Select(const TableSchema& table,
			   int length, string AttrName[], int op[], void* values[],
			   const string& ResultFile);

	Pos Insert(const TableSchema& table, void* values[]);
	//ÒªÅ×Òì³£!!!
	bool Conflict(const TableSchema& table, void* values[]);

	DeletedTuple* Delete(const TableSchema& table, Pos* temptable,
						 int length, string AttrName[], int op[], void* value[]);

	DeletedTuple* Delete(const TableSchema& table,
						 int length, string AttrName[], int op[], void* value[]);

private:
	BufferManager& bm;
	void xhd(const TableSchema& table);
	void PrintAttr(const TableSchema& table, ofstream& fout);
};