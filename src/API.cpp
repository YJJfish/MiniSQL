#include <iostream>
#include <string>

#include "API.h"
#include "Exception.h"
#include "Global.h"

using namespace std;

void API::CreateTable(const TableSchema& NewTable)
{
    //���ɱ��ģʽ�ļ�
    cm.CreateTable(NewTable);
    //��������ʵ����
    IndexSchema index;
    index.IndexName = "__" + NewTable.TableName + "__";
    index.TableName = NewTable.TableName;
    index.AttributeName = NewTable.AttributeName[NewTable.Primary];
    index.Type = NewTable.Type[NewTable.Primary];
    index.AttributeNumber = NewTable.Primary;
    index.Length = NewTable.Length[NewTable.Primary];
    //��������������ģʽ�ļ�
    cm.CreateIndex(NewTable.TableName, index);
    //���ɿձ�
    rm.CreateTable(NewTable);
    //���ɿ�����(��������)
    im.CreateIndex(NewTable, index);
}

void API::DropTable(const string& TableName)
{
    //ɾ���ñ��ȫ�������ļ�
    im.DropIndexOfTable(TableName);
    //ɾ���ñ�������ļ�
    rm.DropTable(TableName);
    //ɾ���ñ��ģʽ�ļ�����������ģʽ�ļ�
    cm.DropTable(TableName);
}

void API::CreateIndex(const TableSchema& NewTable, const IndexSchema& NewIndex)
{
    //�����������ļ�,����B+��
    im.CreateIndex(NewTable,NewIndex);
    //����������ģʽ�ļ�
    cm.CreateIndex(NewTable.TableName, NewIndex);
}

void API::DropIndex(const string& IndexName)
{
    //ɾ�������ļ�
    im.DropIndex(IndexName);
    //ɾ������ģʽ�ļ�
    cm.DropIndex(IndexName);
}

int API::Select(int Counter,const TableSchema& table, int length, string attr[], int op[], void* value[])
{
    //��ѯ�Ż�,��ɸѡ������AttrName[]
    AttrSort(0,length-1,attr,op,value);
    int same_num,remain_num = 0; //��ͬһ��attr�ϵ�����������û��������attr
    string attr_name;
    IndexSchema index;
    Pos *TempTable1 = NULL,*TempTable2 = NULL;
    int i = 0, hasIndex = 0; //hasIndex��¼�Ƿ�������
    string remain_attr[1024]; //û��������������
    int remain_op[1024],same_op[1024]; //û�������Ĳ���������������ͬ�Ĳ�����
    void* remain_value[1024],*same_value[1024]; //û������������ֵ����������ͬ������ֵ
    while (i < length) {
        //��ѯ����i���Ƿ�������
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
            else //RecordManager��������ȡ����
                TempTable1 = rm.Intersect(TempTable1,TempTable2);
        }
        //���û������,����ѯ��������remain������
        else{
            remain_attr[remain_num] = attr[i];
            remain_op[remain_num] = op[i];
            remain_value[remain_num] = value[i];
            remain_num ++;
            i++;
        }
    }
    //ʣ�µ�������record manager����ɸѡ
    string ResultFile = string(ResultDirectory) + "Result" + to_string(Counter) + ".txt";
    int ResultNum;
    if (hasIndex) {
        ResultNum = rm.Select(table, TempTable1, remain_num, remain_attr, remain_op, remain_value, ResultFile);
        //�ͷ������ڴ�
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

//��ɸѡ������AttrName[]����,������ͬ��������ȫ��������
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
    //����Ƿ���������index
    IndexSchema index;
    bool UniqueIndex = 1; //�Ƿ�����unique���Զ���index
    for(int i = 0; i < table.NumAttributes; i++){
        if(table.Unique[i]){
            //���Unique�������Ƿ�������
            if(cm.GetIndex(table.TableName,table.AttributeName[i],index)){
                //���Unique�����Ƿ���ֵ�ظ�
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
                UniqueIndex = 0; //��������Unique�����϶���index
                break;
            }
        }
    }
    //�����Unique����û�н������������������Զ���RecordManager���
    if(!UniqueIndex)
        rm.Conflict(table,values);

    Pos TuplePos = rm.Insert(table,values);
    for(int i = 0; i < table.NumAttributes; i++)
        if(table.Unique[i] && cm.GetIndex(table.TableName, table.AttributeName[i], index))//�Ա��ÿ������,����Ԫ��
            im.Insert(index,values,TuplePos);
}

int API::Delete(const TableSchema& table,int length, string attr[], int op[], void* value[])
{
    //��ѯ�Ż�,��ɸѡ������AttrName[]
    AttrSort(0, length - 1, attr, op, value);
    int same_num,remain_num = 0; //��ͬһ��attr�ϵ�����������û��������attr
    string attr_name;
    IndexSchema index;
    Pos *TempTable1 = NULL,*TempTable2 = NULL;
    int i = 0,hasIndex = 0; //hasIndex��¼�Ƿ�������
    string remain_attr[1024]; //û��������������
    int remain_op[1024],same_op[1024]; //û�������Ĳ���������������ͬ�Ĳ�����
    void* remain_value[1024],*same_value[1024]; //û������������ֵ����������ͬ������ֵ
    while( i < length){
        //��ѯ����i���Ƿ�������
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
            else //RecordManager��������ȡ����
                TempTable1 = rm.Intersect(TempTable1,TempTable2);
        }
            //���û������,����ѯ��������remain������
        else{
            remain_attr[remain_num] = attr[i];
            remain_op[remain_num] = op[i];
            remain_value[remain_num] = value[i];
            remain_num ++;
            i ++;
        }
    }
    //���������ļ�¼��record managerɾ��
    int ResultNum = 0;
    DeletedTuple * deleted;
    if (hasIndex)
        deleted = rm.Delete(table, TempTable1, remain_num, remain_attr, remain_op, remain_value);
    else
        deleted = rm.Delete(table, length, attr, op, value);
    //������ʸñ��ϵ�Index, ����delete
    for (int i = 0; i < table.NumAttributes; i++)
        if (table.Unique[i] && cm.GetIndex(table.TableName, table.AttributeName[i], index))
            im.DeleteTuples(index, deleted);
    //�����������ɾ����¼����, ͬʱ�ͷ��ڴ�
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
