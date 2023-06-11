#include <iostream>

#include "Global.h"
#include "CatalogManager.h"

using namespace std;

bool CatalogManager::TableExists(const string& TableName)
{
    string TableFile = SchemasDirectory + TableName + "/" + TableName + ".Tschema";
    fstream fp(TableFile, ios::in);
    if(fp.fail()) return false;
    else {
        fp.close();
        return true;
    }
}

void CatalogManager::CreateTable(const TableSchema& table)
{
    string TableFile = SchemasDirectory + table.TableName + "/" + table.TableName + ".Tschema";
    Touch(TableFile); //ǿ������Table�ļ��к�TableSchema���ļ�
    fstream fout(TableFile,ios::out | ios::in | ios::binary);
    table.Write(fout);
    fout.close();
}

void CatalogManager::CreateIndex(const string& TableName,const IndexSchema& index)
{
    string IndexFile = SchemasDirectory + TableName + "/" + index.IndexName + ".Ischema";
    Touch(IndexFile);
    fstream fout(IndexFile,ios::out | ios::in | ios::binary);
    index.Write(fout);
    fout.close();
}

void CatalogManager::DropTable(const string& TableName)
{
    Remove(SchemasDirectory + TableName + "/"); //ɾ��table�ļ�������������
}

bool CatalogManager::IndexExists(const string& IndexName)
{
    return FilesExist(SchemasDirectory, IndexName + ".Ischema"); //��֪�����ĸ�table�ļ����£���IndexExists��ֱ�Ӵ��ļ�
}

void CatalogManager::DropIndex(const string& IndexName)
{
    DeleteFiles(SchemasDirectory,IndexName + ".Ischema"); //��֪�����ĸ�table�ļ����£���DeleteFiles����Remove
}

void CatalogManager::GetTable(const string& TableName, TableSchema& table)
{
    string TableFile = SchemasDirectory + TableName + "/" + TableName + ".Tschema";
    fstream fin(TableFile, ios::in | ios::binary);
    table.Read(fin);
    fin.close();
}

bool CatalogManager::GetIndex(const string& TableName, const string& AttrName, IndexSchema& index)
{
    string Path = SchemasDirectory + TableName + "/"; //�ļ���·��
    string format = ".Ischema";
    fstream fin;
    //�ļ����
    intptr_t  hFile = 0;
    //�ļ���Ϣ
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(Path).append("*" + format).c_str(), &fileinfo)) != -1) {
        do {
            string Filename = Path + fileinfo.name;
            fin.open(Filename, ios::in | ios::binary);
            index.Read(fin);
            if (index.AttributeName == AttrName)
                return true;
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    fin.close();
    return false;
}
