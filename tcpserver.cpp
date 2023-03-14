#include "tcpserver.h"
#include "ui_tcpserver.h"
#include<QByteArray>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <qdebug.h>

TcpServer::TcpServer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpServer)
{
    ui->setupUi(this);
//    loadConfig();
    MyTcpServer::getInstance().listen(QHostAddress::Any,8888);

}

TcpServer::~TcpServer()
{
    delete ui;
}

//void TcpServer::loadConfig()
//{
//    QFile file(":/server.config");
//    if (file.open(QIODevice::ReadOnly))
//    {
//        QByteArray baData = file.readAll();
//        QString strData = baData.toStdString().c_str();
//        file.close();
//        strData.replace("\r\n"," ");
//        QStringList strList = strData.split(" ");
//        m_strIP = strList.at(0);
//        m_usPort = strList.at(1).toUShort();
//        qDebug() << "ip:"<< m_strIP <<"Port:"<<m_usPort;
//    }
//    else
//    {
//        QMessageBox::critical(this,"open config","open config failed");
//    }
//}
