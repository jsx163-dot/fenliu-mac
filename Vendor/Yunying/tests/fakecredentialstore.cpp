#include "credentialstore.h"
#include "fakecredentialstore.h"

QString FakeCredentials::username;
QString FakeCredentials::password;
bool FakeCredentials::fail = false;
int FakeCredentials::saves = 0;

void FakeCredentials::reset()
{
    username.clear();
    password.clear();
    fail = false;
    saves = 0;
}

bool CredentialStore::loadLogin(QString &username, QString &password, QString &error)
{
    error = FakeCredentials::fail ? QStringLiteral("Test Keychain is locked") : QString();
    username = FakeCredentials::username;
    password = FakeCredentials::password;
    return !FakeCredentials::fail;
}

bool CredentialStore::saveLogin(const QString &username, const QString &password, QString &error)
{
    ++FakeCredentials::saves;
    if (FakeCredentials::fail) {
        error = QStringLiteral("Test Keychain is locked");
        return false;
    }
    FakeCredentials::username = username;
    FakeCredentials::password = password;
    return true;
}

bool CredentialStore::forgetLogin(QString &error)
{
    if (FakeCredentials::fail) {
        error = QStringLiteral("Test Keychain is locked");
        return false;
    }
    FakeCredentials::username.clear();
    FakeCredentials::password.clear();
    return true;
}
