#include "credentialstore.h"
#include <QJsonDocument>
#include <QJsonObject>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace {
NSMutableDictionary *loginQuery()
{
    return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
              (__bridge id)kSecAttrService: @"com.fenliu.yunying.local",
              (__bridge id)kSecAttrAccount: @"12306-login"} mutableCopy];
}

bool checkStatus(OSStatus status, QString &error)
{
    if (status == errSecSuccess) {
        error.clear();
        return true;
    }
    error = QStringLiteral("钥匙串操作失败 (%1)，请检查钥匙串是否已解锁").arg(status);
    return false;
}
}

bool CredentialStore::loadLogin(QString &username, QString &password, QString &error)
{
    username.clear();
    password.clear();
    NSMutableDictionary *query = loginQuery();
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status == errSecItemNotFound) {
        error.clear();
        return true;
    }
    if (!checkStatus(status, error))
        return false;
    NSData *data = CFBridgingRelease(result);
    QJsonObject object = QJsonDocument::fromJson(
        QByteArray(static_cast<const char *>(data.bytes), int(data.length))).object();
    username = object.value(QStringLiteral("username")).toString();
    password = object.value(QStringLiteral("password")).toString();
    if (username.isEmpty() || password.isEmpty()) {
        error = QStringLiteral("保存的登录信息无效，请重新输入");
        return false;
    }
    return true;
}

bool CredentialStore::saveLogin(const QString &username, const QString &password, QString &error)
{
    QByteArray json = QJsonDocument(QJsonObject{
        {QStringLiteral("username"), username},
        {QStringLiteral("password"), password}
    }).toJson(QJsonDocument::Compact);
    NSData *data = [NSData dataWithBytes:json.constData() length:json.size()];
    NSMutableDictionary *query = loginQuery();
    NSDictionary *updates = @{(__bridge id)kSecValueData: data};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query,
                                   (__bridge CFDictionaryRef)updates);
    if (status == errSecItemNotFound) {
        query[(__bridge id)kSecValueData] = data;
        query[(__bridge id)kSecAttrLabel] = @"分流云映 · 12306 登录";
        query[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
        query[(__bridge id)kSecAttrSynchronizable] = @NO;
        status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
        // Another process may have created the item between update and add.
        if (status == errSecDuplicateItem)
            status = SecItemUpdate((__bridge CFDictionaryRef)loginQuery(),
                                   (__bridge CFDictionaryRef)updates);
    }
    return checkStatus(status, error);
}

bool CredentialStore::forgetLogin(QString &error)
{
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)loginQuery());
    return checkStatus(status == errSecItemNotFound ? errSecSuccess : status, error);
}
