#include "mytcpsocket.h"
#include<qdebug.h>
#include"protocol.h"
#include"mytcpserver.h"
#include<QDir>
#include<QFileInfoList>


MyTcpSocket::MyTcpSocket()
{
    connect(this,SIGNAL(readyRead()),this,SLOT(recvMsg()));
    connect(this,SIGNAL(disconnected()),this,SLOT(clientOffline()));

    m_bUpload = false;
    m_pTimer = new QTimer;
    connect(m_pTimer,SIGNAL(timeout()),
            this,SLOT(sendFileToClient()));
}


QString MyTcpSocket::getName()
{
    return m_strName;
}


void MyTcpSocket::copyDir(QString strSrcDir, QString strDestDir)
{
    QDir dir;
    dir.mkdir(strDestDir);

    dir.setPath(strSrcDir);
    QFileInfoList fileInfoList = dir.entryInfoList();

    QString srcTmp;
    QString destTmp;
    for (int i=0; i<fileInfoList.size(); i++)
    {
        qDebug() << "fileName:" << fileInfoList[i].fileName();
        if (fileInfoList[i].isFile())
        {
            srcTmp = strSrcDir+'/'+fileInfoList[i].fileName();
            destTmp = strDestDir+'/'+fileInfoList[i].fileName();
            QFile::copy(srcTmp, destTmp);
        }
        else if (fileInfoList[i].isDir())
        {
            if (QString(".") == fileInfoList[i].fileName()
                || QString("..") == fileInfoList[i].fileName())
            {
                continue;
            }
            srcTmp = strSrcDir+'/'+fileInfoList[i].fileName();
            destTmp = strDestDir+'/'+fileInfoList[i].fileName();
            copyDir(srcTmp, destTmp);
        }
    }
}

void MyTcpSocket::recvMsg()
{
    if(!m_bUpload)
    {
        qDebug()<<this->bytesAvailable();
        uint uiPDUlen = 0;
        this->read((char*)&uiPDUlen,sizeof(uint));
        uint uiMsglen = uiPDUlen - sizeof(PDU);
        PDU *pdu = mkPDU(uiMsglen);
        this->read((char*)pdu+sizeof(uint),uiPDUlen - sizeof(uint));
        switch (pdu->uiMsgType)
        {
            case ENUM_MSG_TYPE_REGIST_REQUEST:
        {
            char caName[32] = {'\0'};
            char caPwd[32] ={'\0'};
            strncpy(caName,pdu->caData,32);
            strncpy(caPwd,pdu->caData+32,32);
            bool ret = OpeDB::getInstance().handleRegist(caName,caPwd);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;
            if(ret)
            {
                strcpy(respdu->caData,REGIST_OK);
                QDir dir;
                qDebug()<<"create dir: "<<dir.mkdir(QString("./%1").arg(caName));
            }
            else
            {
                strcpy(respdu->caData,REGIST_FAILED);
            }
            this->write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu=NULL;
            break;
        }
            case ENUM_MSG_TYPE_LOGIN_REQUEST:
        {
            char caName[32] = {'\0'};
            char caPwd[32] ={'\0'};
            strncpy(caName,pdu->caData,32);
            strncpy(caPwd,pdu->caData+32,32);
            bool ret = OpeDB::getInstance().handleLogin(caName,caPwd);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;
            if(ret)
            {
                strcpy(respdu->caData,LOGIN_OK);
                m_strName = caName;
            }
            else
            {
                strcpy(respdu->caData,LOGIN_FAILED);
            }
            this->write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu=NULL;
            break;
        }
            case ENUM_MSG_TYPE_ALL_ONLINE_REQUEST:
        {
            QStringList ret = OpeDB::getInstance().handleAllOnline();
            uint uiMsglen = ret.size()*32;
            PDU *respdu = mkPDU(uiMsglen);
            respdu->uiMsgType = ENUM_MSG_TYPE_ALL_ONLINE_RESPOND;
            for(int i=0;i<ret.size();i++)
            {
                memcpy((char*)(respdu->caMsg)+i*32,ret.at(i).toStdString().c_str(),ret.at(i).size());
            }
            this->write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu=NULL;
            break;
        }
            case ENUM_MSG_TYPE_SEARCH_USR_REQUEST:
        {
            int ret = OpeDB::getInstance().handlSearchUsr(pdu->caData);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType=ENUM_MSG_TYPE_SEARCH_USR_RESPOND;
            if(-1 == ret)
            {
                strcpy(respdu->caData,SEARCH_USR_NONE);
            }
            else if(1 == ret)
            {
                strcpy(respdu->caData,SEARCH_USR_ONLINE);
            }
            else if(0 == ret)
            {
                strcpy(respdu->caData,SEARCH_USR_OFFLINE);
            }
            this->write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu=NULL;
            break;
        }
            case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST:
        {
            char caPerName[32] = {'\0'};
            char caName[32] ={'\0'};
            strncpy(caPerName,pdu->caData,32); //对方的名字
            strncpy(caName,pdu->caData+32,32);  //自己的名字
            int ret = OpeDB::getInstance().handleAddFriend(caPerName,caName);
            PDU *respdu =mkPDU(0);
            if(-1 == ret) //名字有误
            {
                respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
                strcpy(respdu->caData,UNKNOW_ERROR);
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            else if(0 == ret) // 已经是好友
            {
                respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
                strcpy(respdu->caData,EXISTED_FRIEND);
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            else if(1 == ret) //在线,需要发送申请请求
            {
                MyTcpServer::getInstance().resend(caPerName,pdu);
            }
            else  if(2 == ret) //不在线
            {
                respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
                strcpy(respdu->caData,ADD_FRIEND_OFFLINE);
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            else if(3 == ret) //不存在
            {
                respdu = mkPDU(0);
                respdu->uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
                strcpy(respdu->caData,SEARCH_USR_NONE);
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            break;
        }
            case ENUM_MSG_TYPE_ADD_FRIEND_AGREE: //同意添加好友
            {
                char caPerName[32] = {'\0'}; //对方的名字
                char caName[32] ={'\0'};    //自己的名字
                strncpy(caName,pdu->caData,32); //对方的名字
                strncpy(caPerName,pdu->caData+32,32);  //自己的名字
                OpeDB::getInstance().handAddFriendAgree(caPerName,caName);
                MyTcpServer::getInstance().resend(caName,pdu);
                break;
            }
            case ENUM_MSG_TYPE_ADD_FRIEND_REFUSE: //拒绝了好友申请
            {
                char caName[32] = {'\0'};
                strncpy(caName, pdu->caData+32, 32);
                MyTcpServer::getInstance().resend(caName, pdu);
                break;
            }
            case ENUM_MSG_TYPE_FLUSH_FRIEND_REQUEST:
        {
            char caName[32] = {'\0'};
            strncpy(caName, pdu->caData, 32);
            QStringList ret = OpeDB::getInstance().handleFlushFriend(caName);
            uint uiMsglen = ret.size()*32;
            PDU *respdu = mkPDU(uiMsglen);
            respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
            for(int i=0 ;i<ret.size();i++)
            {
                memcpy((char*)(respdu->caMsg)+i*32,ret.at(i).toStdString().c_str(),ret.at(i).size());
            }
            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
            break;
        }
            case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST:
        {
            char caSelName[32] = {'\0'}; //自己的名字
            char caFriendName[32] ={'\0'};    //好友的名字
            strncpy(caSelName,pdu->caData,32); //自己的名字
            strncpy(caFriendName,pdu->caData+32,32);  //好友的名字
            OpeDB::getInstance().handleDelFriend(caSelName,caFriendName);

            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
            strcpy(respdu->caData,DEL_FRIEND_OK);
            write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;

            MyTcpServer::getInstance().resend(caFriendName,pdu);

            break;
        }
            case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST:
        {
            char caPerName[32] = {'\0'};
            memcpy(caPerName, pdu->caData+32, 32);
            char caName[32] = {'\0'};
            memcpy(caName, pdu->caData, 32);
            qDebug() << caName << "-->" << caPerName << (char*)(pdu->caMsg);
            MyTcpServer::getInstance().resend(caPerName, pdu);

            break;
        }
            case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST:
        {
            char caName[32] = {'\0'};
            strncpy(caName,pdu->caData,32);
            QStringList onlineFriend = OpeDB::getInstance().handleFlushFriend(caName); //获取caName的在线好友
            QString tmp;
            for(int i=0;i<onlineFriend.size();i++)
            {
                tmp = onlineFriend.at(i);
                MyTcpServer::getInstance().resend(tmp.toStdString().c_str(),pdu);
            }
            break;
        }
            case ENUM_MSG_TYPE_CREATE_DIR_REQUEST:
        {
            QDir dir;
            QString strCurPath = QString("%1").arg((char*)(pdu->caMsg));
            qDebug() << strCurPath;
            bool ret = dir.exists(strCurPath);
            PDU *respdu = NULL;
            if (ret)  //当前目录存在
            {
                 char caNewDir[32] = {'\0'};
                 memcpy(caNewDir, pdu->caData+32, 32);
                 QString strNewPath = strCurPath+"/"+caNewDir;
                 qDebug() << strNewPath;
                 ret = dir.exists(strNewPath);
                 qDebug() << "-->" << ret;
                 if (ret)  //创建的文件名已存在
                 {
                      respdu = mkPDU(0);
                      respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;
                      strcpy(respdu->caData, FILE_NAME_EXIST);
                 }
                 else   //创建的文件名不存在
                 {
                      dir.mkdir(strNewPath);
                      respdu = mkPDU(0);
                      respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;
                      strcpy(respdu->caData, CREAT_DIR_OK);
                 }
             }
             else     //当前目录不存在
             {
                  respdu = mkPDU(0);
                  respdu->uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;
                  strcpy(respdu->caData, DIR_NO_EXIST);
              }
              write((char*)respdu, respdu->uiPDUlen);
              free(respdu);
              respdu = NULL;
              break;
        }
            case ENUM_MSG_TYPE_FLUSH_FILE_REQUEST:  //可以改动
        {
            char *pCurPath = new char[pdu->uiMsglen];
            memcpy(pCurPath,pdu->caMsg,pdu->uiMsglen); //接受当前路径
            QDir dir(pCurPath);
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
            QFileInfoList fileInfoList = dir.entryInfoList(); //以列表形式返回

            int iFileCount = fileInfoList.size();
            PDU *respdu = mkPDU(sizeof(FileInfo)*iFileCount);
            respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
            FileInfo *pFileInfo = NULL;
            QString strFileName;
            qDebug() <<"文件列表数量" << fileInfoList.size();
            for(int i=0;i<fileInfoList.size();i++)
            {

                pFileInfo = (FileInfo *)(respdu->caMsg)+i;
                strFileName = fileInfoList[i].fileName();
                qDebug() << strFileName;
                memcpy(pFileInfo->caName,strFileName.toStdString().c_str(),strFileName.size());
                if(fileInfoList[i].isDir())
                {
                    pFileInfo->iFileType = 0; //表示目录
                }
                else if(fileInfoList[i].isFile())
                {
                    pFileInfo->iFileType = 1; //表示常规文件
                }
            }
            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
            break;
        }
        case ENUM_MSG_TYPE_DEL_DIR_REQUEST:
        {
            char caName[32] = {'\0'};
            strcpy(caName,pdu->caData);
            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen);
            QString strPath = QString("%1/%2").arg(pPath).arg(caName);
            qDebug() <<strPath;

            QFileInfo fileInfo(strPath);
            bool ret = false;
            if(fileInfo.isDir())
            {
                QDir dir;
                dir.setPath(strPath);
                ret = dir.removeRecursively();
            }
            else if(fileInfo.isFile())
            {
                ret = false;
            }
            PDU *respdu = NULL;
            if(ret)
            {
                respdu = mkPDU(strlen(DEL_DIR_OK)+1);
                respdu->uiMsgType = ENUM_MSG_TYPE_DEL_DIR_RESPOND;
                memcpy(respdu->caData,DEL_DIR_OK,strlen(DEL_DIR_OK));
            }
            else
            {
                respdu = mkPDU(strlen(DEL_DIR_FAILURED)+1); //这个地方不一样
                respdu->uiMsgType = ENUM_MSG_TYPE_DEL_DIR_RESPOND;
                memcpy(respdu->caData,DEL_DIR_FAILURED,strlen(DEL_DIR_FAILURED));
            }
            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
            break;
        }
        case ENUM_MSG_TYPE_RENAME_FILE_REQUEST:
        {
            char caOldName[32] = {'\0'};
            char caNewName[32] = {'\0'};
            strncpy(caOldName,pdu->caData,32);
            strncpy(caNewName,pdu->caData+32,32);

            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen);

            QString strOldPath = QString("%1/%2").arg(pPath).arg(caOldName);
            QString strNewPath = QString("%1/%2").arg(pPath).arg(caNewName);

            QDir dir;
            bool ret = dir.rename(strOldPath,strNewPath);
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_RENAME_FILE_RESPOND;
            if(ret)
            {
                strcpy(respdu->caData,RENAME_FILE_OK);
            }
            else
            {
                strcpy(respdu->caData,RENAME_FILE_FAILED);
            }
            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
            break;
        }
        case ENUM_MSG_TYPE_ENTER_DIR_REQUEST:
        {
            char caEnterName[32] = {'\0'};
            strncpy(caEnterName,pdu->caData,32); //获得双击的目录

            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen); //当前路径

            QString strPath = QString("%1/%2").arg(pPath).arg(caEnterName); //拼接后

            QFileInfo fileInfo(strPath);
            PDU *respdu = NULL;
            if(fileInfo.isDir())
            {
                QDir dir(strPath);
                QFileInfoList fileInfoList = dir.entryInfoList();

                int iFileCount = fileInfoList.size();
                respdu = mkPDU(sizeof(FileInfo)*iFileCount);
                respdu->uiMsgType = ENUM_MSG_TYPE_FLUSH_FILE_RESPOND;
                FileInfo *pFileInfo = NULL;
                QString strFileName;
                qDebug() << fileInfoList.size();
                for(int i=0;i<fileInfoList.size();i++)
                {
                    pFileInfo = (FileInfo *)(respdu->caMsg)+i;
                    strFileName = fileInfoList[i].fileName();
                    qDebug() << strFileName;
                    memcpy(pFileInfo->caName,strFileName.toStdString().c_str(),strFileName.size());
                    if(fileInfoList[i].isDir())
                    {
                        pFileInfo->iFileType = 0; //表示目录
                    }
                    else if(fileInfoList[i].isFile())
                    {
                        pFileInfo->iFileType = 1; //表示常规文件
                    }
                }
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            else if(fileInfo.isFile())
            {
                respdu = mkPDU(0);
                respdu->uiMsgType=ENUM_MSG_TYPE_ENTER_DIR_RESPOND;
                strcpy(respdu->caData,ENTER_DIR_FAILURED);
                write((char*)respdu, respdu->uiPDUlen);
                free(respdu);
                respdu = NULL;
            }
            break;
        }
        case ENUM_MSG_TYPE_UPLOAD_FILE_REQUEST:
        {
            char caFileName[32] = {'\0'};
            qint64 fileSize = 0;
            sscanf(pdu->caData,"%s %lld", caFileName, &fileSize); //获得双击的目录
            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen); //当前路径
            QString strPath = QString("%1/%2").arg(pPath).arg(caFileName); //拼接后
            qDebug() << strPath;
            delete []pPath;
            pPath = NULL;

            m_file.setFileName(strPath);
            //以只写的方式打开文件，若文件不存在，则会自动创建文件
            if(m_file.open(QIODevice::WriteOnly))
            {
                m_bUpload = true;
                m_iTotal = fileSize;
                m_iRecved = 0;
            }
            break;
        }
        case ENUM_MSG_TYPE_DEL_FILE_REQUEST:
        {
            char caName[32] = {'\0'};
            strcpy(caName,pdu->caData);
            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen);
            QString strPath = QString("%1/%2").arg(pPath).arg(caName);
            qDebug() <<strPath;

            QFileInfo fileInfo(strPath);
            bool ret = false;
            if(fileInfo.isDir())
            {
                ret = false;
            }
            else if(fileInfo.isFile())
            {
                QDir dir;
                ret = dir.remove(strPath);
            }
            PDU *respdu = NULL;
            if(ret)
            {
                respdu = mkPDU(0);
//                respdu = mkPDU(strlen(DEL_FILE_OK)+1);
                respdu->uiMsgType = ENUM_MSG_TYPE_DEL_FILE_RESPOND;
                memcpy(respdu->caData,DEL_FILE_OK,strlen(DEL_FILE_OK));
            }
            else
            {
                respdu = mkPDU(0);
//                respdu = mkPDU(strlen(DEL_FILE_FAILURED)+1); //这个地方不一样
                respdu->uiMsgType = ENUM_MSG_TYPE_DEL_FILE_RESPOND;
                memcpy(respdu->caData,DEL_FILE_FAILURED,strlen(DEL_FILE_FAILURED));
            }
            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
            break;
        }
        case ENUM_MSG_TYPE_DOWNLOAD_FILE_REQUEST:
        {
            char caFileName[32] = {'\0'};
            strcpy(caFileName,pdu->caData);
            char *pPath = new char[pdu->uiMsglen];
            memcpy(pPath,pdu->caMsg,pdu->uiMsglen); //当前路径
            QString strPath = QString("%1/%2").arg(pPath).arg(caFileName); //拼接后
            qDebug() << strPath;
            delete []pPath;
            pPath = NULL;

            QFileInfo fileInfo(strPath);
            qint64 fileSize = fileInfo.size();
            PDU *respdu = mkPDU(0);
            respdu->uiMsgType = ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPOND;
            sprintf(respdu->caData,"%s %lld",caFileName ,fileSize);

            write((char*)respdu, respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;

            m_pTimer->start(1000);

            break;
        }
        case ENUM_MSG_TYPE_SHARE_FILE_REQUEST:
        {
             char caSendName[32] = {'\0'};
             int num = 0;
             sscanf(pdu->caData, "%s%d", caSendName, &num); //保存分享者的名字
             int size = num*32;
             PDU *respdu = mkPDU(pdu->uiMsglen-size);
             respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_NOTE;
             strcpy(respdu->caData, caSendName);
             memcpy(respdu->caMsg, (char*)(pdu->caMsg)+size, pdu->uiMsglen-size);

             char caRecvName[32] = {'\0'};
             for (int i=0; i<num; i++)
             {
                  memcpy(caRecvName, (char*)(pdu->caMsg)+i*32, 32);
                  MyTcpServer::getInstance().resend(caRecvName, respdu);
             }
             free(respdu);
             respdu = NULL;

             respdu = mkPDU(0);
             respdu->uiMsgType = ENUM_MSG_TYPE_SHARE_FILE_RESPOND;
             strcpy(respdu->caData, "share file ok");
             write((char*)respdu, respdu->uiPDUlen);
             free(respdu);
             respdu = NULL;

             break;
         }
        case ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPOND:
        {
        QString strRecvPath = QString("./%1").arg(pdu->caData);
        QString strShareFilePath = QString("%1").arg((char*)(pdu->caMsg));
        int index = strShareFilePath.lastIndexOf('/');
        QString strFileName = strShareFilePath.right(strShareFilePath.size()-index-1);
        strRecvPath = strRecvPath+'/'+strFileName;

        QFileInfo fileInfo(strShareFilePath);
        if (fileInfo.isFile())
        {
           QFile::copy(strShareFilePath, strRecvPath);
        }
        else if (fileInfo.isDir())
        {
           copyDir(strShareFilePath, strRecvPath);
        }
           break;
        }
        default:
            break;
        }
        free(pdu);
        pdu=NULL;
    }
    else
    {
        QByteArray buff = readAll();
        m_file.write(buff);
        m_iRecved += buff.size();

        PDU *respdu = mkPDU(0);
        respdu->uiMsgType = ENUM_MSG_TYPE_UPLOAD_FILE_RESPOND;
        if(m_iTotal == m_iRecved)
        {
            m_file.close();
            m_bUpload = false;
            strcpy(respdu->caData,UPLOAD_FILE_OK);
            write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
        }
        else if(m_iTotal < m_iRecved)
        {
            m_file.close();
            m_bUpload = false;
            strcpy(respdu->caData,UPLOAD_FILE_FAILURED);
            write((char*)respdu,respdu->uiPDUlen);
            free(respdu);
            respdu = NULL;
        }
    }
    //    qDebug() <<caName <<caPwd <<pdu->uiMsgType;
}

void MyTcpSocket::clientOffline()
{
    OpeDB::getInstance().handleOffline(m_strName.toStdString().c_str());
    emit offline(this); //emit发送信号
}

void MyTcpSocket::sendFileToClient()
{
    char *pData = new char[4096];
    qint64 ret = 0;
    while(true)
    {
        ret = m_file.read(pData,4096);
        if(ret>0 && ret<=4096)
        {
            write(pData,ret);
        }
        else if(0 == ret)
        {
            m_file.close();
            break;
        }
//        else if(ret <0)
//        {
//            qDebug() <<"发送文件给客户端失败";
//            m_file.close();
//            break;
//        }
    }
    delete [] pData;
    pData = NULL;
}
