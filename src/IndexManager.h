#pragma once
#include <iostream>
#include "Global.h"
#include "BufferManager.h"
using namespace std;

class IndexManager {
public:
	IndexManager(BufferManager& bm) :bm(bm) {}
	~IndexManager(void) {}
	//��������, ����table�����е����ݹ���B+��
	void CreateIndex(const TableSchema& table, const IndexSchema& index);
	//ɾ��TableName������������ļ�
	void DropIndexOfTable(const string& TableName);
	//ɾ����������
	void DropIndex(const string& IndexName);
	//��ѯ, ��������. ��֤����Ԫ�ذ�(��ƫ����, �ֽ�ƫ����)����
	Pos* Select(const IndexSchema& index,
				int length, int op[], void* value[]);
	//���value�Ƿ��Ѿ���B+���д���
	bool Conflict(const IndexSchema& index, void* value);
	//��B+���в�����
	void Insert(const IndexSchema& index, void* values[], const Pos& position);
	//��B+����ɾ��list����Ӧ��һϵ�н��
	void DeleteTuples(const IndexSchema& index, DeletedTuple* list);
private:
	//Buffer Manager
	BufferManager& bm;
	//��B+����ɾ���������
	void Delete(const IndexSchema& index, DeletedTuple* node);
	//�ӿ����б��еõ����п�
	Block* GetNewBlock(const IndexSchema& index);
	//�����п��������б�
	void DeleteBlock(const IndexSchema& index, Block* Obj);
};