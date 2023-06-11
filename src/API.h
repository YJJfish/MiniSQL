#pragma once
#include <iostream>
#include "Global.h"
#include "CatalogManager.h"
#include "RecordManager.h"
#include "IndexManager.h"

using namespace std;

class API {
public:
	API(CatalogManager& cm, RecordManager& rm, IndexManager& im) : cm(cm), rm(rm), im(im) {}
	~API(void) {}

	char* Message;  //执行返回信息
	int Exception;  //执行是否有异常

	/*
	 * 函数：CreateTable
	 * 参数：新表的模式信息
	 * 调用CatalogManager, RecordManager, IndexManager, 新建表
	 */
	void CreateTable(const TableSchema& NewTable);

	/*
	 * 函数：DropTable
	 * 参数：表名
	 * 调用CatalogManager, RecordManager, IndexManager, 删除表
	 */
	void DropTable(const string& TableName);

	/*
	 * 函数:CreateIndex
	 *
	 */
	void CreateIndex(const TableSchema& NewTable, const IndexSchema& NewIndex);

	/*
	 * 函数：dropIndex
	 */
	void DropIndex(const string& IndexName);

	/*
	 * 函数：Select
	 * 参数：Table模式信息，三个数组存放conditions
	 */
	int Select(int Counter,const TableSchema& table, int length, string attr[], int op[], void* value[]);

	/*
	 * 函数：Insert
	 * 参数：Table模式信息
	 */
	void Insert(const TableSchema& table, void* values[]);

	/*
	 * 函数：Delete
	 * 参数：Table模式信息，三个数组存放conditions
	 */
	int Delete(const TableSchema& table,
				int length, string attr[], int op[], void* value[]);

private:
	//引用全局catalog manager
	CatalogManager& cm;
	//引用全局record manager
	RecordManager& rm;
	//引用全局index manager
	IndexManager& im;

    void AttrSort(int l,int r,string attr[],int op[],void* value[]);
};

