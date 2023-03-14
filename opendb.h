#ifndef OPENDB_H
#define OPENDB_H

#include <QObject>

class OpenDB : public QObject
{
    Q_OBJECT
public:
    explicit OpenDB(QObject *parent = 0);
    static OpenDB& getInstance();

signals:

};

#endif // OPENDB_H
