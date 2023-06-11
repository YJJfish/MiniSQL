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

	char* Message;  //ִ�з�����Ϣ
	int Exception;  //ִ���Ƿ����쳣

	/*
	 * ������CreateTable
	 * �������±��ģʽ��Ϣ
	 * ����CatalogManager, RecordManager, IndexManager, �½���
	 */
	void CreateTable(const TableSchema& NewTable);

	/*
	 * ������DropTable
	 * ����������
	 * ����CatalogManager, RecordManager, IndexManager, ɾ����
	 */
	void DropTable(const string& TableName);

	/*
	 * ����:CreateIndex
	 *
	 */
	void CreateIndex(const TableSchema& NewTable, const IndexSchema& NewIndex);

	/*
	 * ������dropIndex
	 */
	void DropIndex(const string& IndexName);

	/*
	 * ������Select
	 * ������Tableģʽ��Ϣ������������conditions
	 */
	int Select(int Counter,const TableSchema& table, int length, string attr[], int op[], void* value[]);

	/*
	 * ������Insert
	 * ������Tableģʽ��Ϣ
	 */
	void Insert(const TableSchema& table, void* values[]);

	/*
	 * ������Delete
	 * ������Tableģʽ��Ϣ������������conditions
	 */
	int Delete(const TableSchema& table,
				int length, string attr[], int op[], void* value[]);

private:
	//����ȫ��catalog manager
	CatalogManager& cm;
	//����ȫ��record manager
	RecordManager& rm;
	//����ȫ��index manager
	IndexManager& im;

    void AttrSort(int l,int r,string attr[],int op[],void* value[]);
};

