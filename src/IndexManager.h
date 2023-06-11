#pragma once
#include <iostream>
#include "Global.h"
#include "BufferManager.h"
using namespace std;

class IndexManager {
public:
	IndexManager(BufferManager& bm) :bm(bm) {}
	~IndexManager(void) {}
	//创建索引, 根据table中已有的内容构建B+树
	void CreateIndex(const TableSchema& table, const IndexSchema& index);
	//删除TableName表的所有索引文件
	void DropIndexOfTable(const string& TableName);
	//删除单个索引
	void DropIndex(const string& IndexName);
	//查询, 返回链表. 保证链表元素按(块偏移量, 字节偏移量)排序
	Pos* Select(const IndexSchema& index,
				int length, int op[], void* value[]);
	//检查value是否已经在B+树中存在
	bool Conflict(const IndexSchema& index, void* value);
	//在B+树中插入结点
	void Insert(const IndexSchema& index, void* values[], const Pos& position);
	//在B+树中删除list所对应的一系列结点
	void DeleteTuples(const IndexSchema& index, DeletedTuple* list);
private:
	//Buffer Manager
	BufferManager& bm;
	//在B+树中删除单个结点
	void Delete(const IndexSchema& index, DeletedTuple* node);
	//从空闲列表中得到空闲块
	Block* GetNewBlock(const IndexSchema& index);
	//将空闲块挂入空闲列表
	void DeleteBlock(const IndexSchema& index, Block* Obj);
};