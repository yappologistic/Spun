#pragma once
#include <QString>
class Player;
class Youtube;
class QQuickWindow;
int exerciseYoutube(Player &local, Youtube &youtube, QQuickWindow *window,
                    const QString &temp, const QString &captures);

int exerciseYoutubeLive(Player &local, Youtube &youtube, QQuickWindow *window,
                        const QString &captures);
