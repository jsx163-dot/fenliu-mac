#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include <QString>

namespace CredentialStore {
bool loadLogin(QString &username, QString &password, QString &error);
bool saveLogin(const QString &username, const QString &password, QString &error);
bool forgetLogin(QString &error);
}

#endif
