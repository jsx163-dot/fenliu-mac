#ifndef FAKECREDENTIALSTORE_H
#define FAKECREDENTIALSTORE_H
#include <QString>
namespace FakeCredentials {
extern QString username;
extern QString password;
extern bool fail;
extern int saves;
void reset();
}
#endif
