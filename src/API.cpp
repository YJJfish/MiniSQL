#include <iostream>
#include <string>

#include "API.h"
#include "Exception.h"
#include "Global.h"

using namespace std;

void API::CreateTable(const TableSchema& NewTable)
{
    //生成表的模式文件
    cm.CreateTable(NewTable);
    //主键索引实例化
    IndexSchema index;
    index.IndexName = "__" + NewTable.TableName + "__";
    index.TableName = NewTable.TableName;
    index.AttributeName = NewTable.AttributeName[NewTable.Primary];
    index.Type = NewTable.Type[NewTable.Primary];
    index.AttributeNumber = NewTable.Primary;
    index.Length = NewTable.Length[NewTable.Primary];
    //生成主键索引的模式文件
    cm.CreateIndex(NewTable.TableName, index);
    //生成空表
    rm.CreateTable(NewTable);
    //生成空索引(主键索引)
    im.CreateIndex(NewTable, index);
}

void API::DropTable(const string& TableName)
{
    //删除该表的全部索引文件
    im.DropIndexOfTable(TableName);
    //删除该表的数据文件
    rm.DropTable(TableName);
    //删除该表的模式文件及所有索引模式文件
    cm.DropTable(TableName);
}

void API::CreateIndex(const TableSchema& NewTable, const IndexSchema& NewIndex)
{
    //生成主索引文件,构建B+树
    im.CreateIndex(NewTable,NewIndex);
    //生成索引的模式文件
    cm.CreateIndex(NewTable.TableName, NewIndex);
}

void API::DropIndex(const string& IndexName)
{
    //删除索引文件
    im.DropIndex(IndexName);
    //删除索引模式文件
    cm.DropIndex(IndexName);
}

int API::Select(int Counter,const TableSchema& table, int length, string attr[], int op[], void* value[])
{
    //查询优化,将筛选条件按AttrName[]
    AttrSort(0,length-1,attr,op,value);
    int same_num,remain_num = 0; //在同一个attr上的条件数量和没有索引的attr
    string attr_name;
    IndexSchema index;
    Pos *TempTable1 = NULL,*TempTable2 = NULL;
    int i = 0, hasIndex = 0; //hasIndex记录是否有索引
    string remain_attr[1024]; //没有索引的属性名
    int remain_op[1024],same_op[1024]; //没有索引的操作符和索引名相同的操作符
    void* remain_value[1024],*same_value[1024]; //没有索引的属性值和索引名相同的属性值
    while (i < length) {
        //查询属性i上是否有索引
        if (table.Unique[i] && cm.GetIndex(table.TableName, attr[i], index)) {
            hasIndex = 1;
            same_num = 0;
            attr_name = attr[i];
            do{
                same_op[same_num] = op[i];
                same_value[same_num] = value[i];
                same_num ++;
                i ++;
            }while(i < length && attr[i] == attr_name);
            TempTable2 = im.Select(index,same_num,same_op,same_value);
            if(TempTable1 == NULL)
                TempTable1 = TempTable2;
            else //RecordManager对两个表取交集
                TempTable1 = rm.Intersect(TempTable1,TempTable2);
        }
        //如果没有索引,将查询条件放入remain数组中
        else{
            remain_attr[remain_num] = attr[i];
            remain_op[remain_num] = op[i];
            remain_value[remain_num] = value[i];
            remain_num ++;
            i++;
        }
    }
    //剩下的条件用record manager进行筛选
    string ResultFile = string(ResultDirectory) + "Result" + to_string(Counter) + ".txt";
    int ResultNum;
    if (hasIndex) {
        ResultNum = rm.Select(table, TempTable1, remain_num, remain_attr, remain_op, remain_value, ResultFile);
        //释放链表内存
        Pos* ptr1 = TempTable1, * ptr2;
        while (ptr1) {
            ptr2 = ptr1->Next;
            delete ptr1;
            ptr1 = ptr2;
        }
    }
    else
        ResultNum = rm.Select(table, length, attr, op, value, ResultFile);
    return ResultNum;
}

//将筛选条件按AttrName[]排序,这样相同的属性名全部相邻了
void API::AttrSort(int l,int r,string attr[],int op[],void* value[])
{
    string m = attr[(l + r) / 2];
    int i = l, j = r;
    while (i <= j) {
        while (attr[i] < m)i++;
        while (m < attr[j])j--;
        if (i <= j) {
            string TempAttr = attr[i];
            int TempOp = op[i];
            void * TempValue = value[i];
            attr[i] = attr[j];
            op[i] = op[j];
            value[i] = value[j];
            attr[j] = TempAttr;
            op[j] = TempOp;
            value[j] = TempValue;
            i++; j--;
        }
    }
    if (l < j) AttrSort( l, j, attr, op, value);
    if (i < r) AttrSort(i, r, attr, op, value);
}

void API::Insert(const TableSchema& table, void* values[])
{
    //检查是否有属性有index
    IndexSchema index;
    bool UniqueIndex = 1; //是否所有unique属性都有index
    for(int i = 0; i < table.NumAttributes; i++){
        if(table.Unique[i]){
            //检查Unique属性上是否有索引
            if(cm.GetIndex(table.TableName,table.AttributeName[i],index)){
                //检查Unique属性是否有值重复
                if (im.Conflict(index, values[i])) {
                    if (table.Type[i] == SQL_INT)
                        throw InsertionError(table.AttributeName[i], to_string(*(int*)values[i]));
                    else if (table.Type[i] == SQL_FLOAT)
                        throw InsertionError(table.AttributeName[i], to_string(*(float*)values[i]));
                    else
                        throw InsertionError(table.AttributeName[i], *(string*)values[i]);
                }
            }
            else{
                UniqueIndex = 0; //不是所有Unique属性上都有index
                break;
            }
        }
    }
    //如果有Unique属性没有建立过索引，所有属性都用RecordManager检查
    if(!UniqueIndex)
        rm.Conflict(table,values);

    Pos TuplePos = rm.Insert(table,values);
    for(int i = 0; i < table.NumAttributes; i++)
        if(table.Unique[i] && cm.GetIndex(table.TableName, table.AttributeName[i], index))//对表的每个索引,插入元组
            im.Insert(index,values,TuplePos);
}

int API::Delete(const TableSchema& table,int length, string attr[], int op[], void* value[])
{
    //查询优化,将筛选条件按AttrName[]
    AttrSort(0, length - 1, attr, op, value);
    int same_num,remain_num = 0; //在同一个attr上的条件数量和没有索引的attr
    string attr_name;
    IndexSchema index;
    Pos *TempTable1 = NULL,*TempTable2 = NULL;
    int i = 0,hasIndex = 0; //hasIndex记录是否有索引
    string remain_attr[1024]; //没有索引的属性名
    int remain_op[1024],same_op[1024]; //没有索引的操作符和索引名相同的操作符
    void* remain_value[1024],*same_value[1024]; //没有索引的属性值和索引名相同的属性值
    while( i < length){
        //查询属性i上是否有索引
        if(cm.GetIndex(table.TableName,attr[i],index)){
            hasIndex = 1;
            same_num = 0;
            attr_name = attr[i];
            do{
                same_op[same_num] = op[i];
                same_value[same_num] = value[i];
                same_num ++;
                i ++;
            }while(i < length && attr[i] == attr_name);
            TempTable2 = im.Select(index,same_num,same_op,same_value);
            if(TempTable1 == NULL)
                TempTable1 = TempTable2;
            else //RecordManager对两个表取交集
                TempTable1 = rm.Intersect(TempTable1,TempTable2);
        }
            //如果没有索引,将查询条件放入remain数组中
        else{
            remain_attr[remain_num] = attr[i];
            remain_op[remain_num] = op[i];
            remain_value[remain_num] = value[i];
            remain_num ++;
            i ++;
        }
    }
    //符合条件的记录用record manager删除
    int ResultNum = 0;
    DeletedTuple * deleted;
    if (hasIndex)
        deleted = rm.Delete(table, TempTable1, remain_num, remain_attr, remain_op, remain_value);
    else
        deleted = rm.Delete(table, length, attr, op, value);
    //逐个访问该表上的Index, 进行delete
    for (int i = 0; i < table.NumAttributes; i++)
        if (table.Unique[i] && cm.GetIndex(table.TableName, table.AttributeName[i], index))
            im.DeleteTuples(index, deleted);
    //遍历链表计算删除记录个数, 同时释放内存
    DeletedTuple* ptr;
    while(deleted != NULL){
        ResultNum++;
        ptr = deleted->Next;
        for (int i = 0; i < table.NumAttributes; i++)
            delete deleted->values[i];
        delete deleted->values;
        delete deleted;
        deleted = ptr;
    }
    return ResultNum;
}
