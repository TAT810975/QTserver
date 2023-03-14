#include "opendb.h"

OpenDB::OpenDB(QObject *parent) : QObject(parent)
{

}

OpenDB &OpenDB::getInstance()
{
    static OpenDB instance;
    return instance;
}
