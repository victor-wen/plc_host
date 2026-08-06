#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include "logging/LogService.h"

class LogServiceTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    QString readAll(const QString& name) const
    {
        QFile f(m_dir.path() + QLatin1Char('/') + name);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        const QString content = QString::fromUtf8(f.readAll());
        f.close();
        return content;
    }

private slots:
    void appLog_matches_specified_format()
    {
        LogService svc(m_dir.path());
        svc.logApp(LogService::Level::Info, QStringLiteral("hello world"));

        const QString content = readAll(QStringLiteral("app.log"));
        const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 1);

        const QRegularExpression re(QStringLiteral(
            "^\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\] \\[INFO\\] hello world$"));
        QVERIFY2(re.match(lines[0]).hasMatch(), qPrintable(lines[0]));
    }

    void operationLog_contains_tag_values_and_levels()
    {
        LogService svc(m_dir.path());
        svc.logOperation(7, QVariant(1.0), QVariant(2.5), true, QString());
        svc.logOperation(8, QVariant(0), QVariant(1), false, QStringLiteral("timeout"));

        const QString content = readAll(QStringLiteral("operation.log"));
        QVERIFY(content.contains(QStringLiteral("tagId=7")));
        QVERIFY(content.contains(QStringLiteral("old=1")));
        QVERIFY(content.contains(QStringLiteral("new=2.5")));
        QVERIFY(content.contains(QStringLiteral("success=true")));
        QVERIFY(content.contains(QStringLiteral("[INFO]")));
        QVERIFY(content.contains(QStringLiteral("tagId=8")));
        QVERIFY(content.contains(QStringLiteral("success=false")));
        QVERIFY(content.contains(QStringLiteral("error=timeout")));
        QVERIFY(content.contains(QStringLiteral("[ERROR]")));
    }

    void rotation_produces_1_and_2_generations()
    {
        LogService svc(m_dir.path());
        svc.setMaxFileSizeBytes(256);

        const QString longMsg = QString(120, QLatin1Char('x'));
        for (int i = 0; i < 10; ++i)
            svc.logApp(LogService::Level::Info, longMsg);

        const QString dirPath = m_dir.path();
        QVERIFY(QFileInfo(dirPath + QLatin1Char('/') + QStringLiteral("app.log")).exists());
        QVERIFY(QFileInfo(dirPath + QLatin1Char('/') + QStringLiteral("app.1.log")).exists());
        QVERIFY(QFileInfo(dirPath + QLatin1Char('/') + QStringLiteral("app.2.log")).exists());

        // 最新一条记录保留在当前文件
        QVERIFY(readAll(QStringLiteral("app.log")).contains(longMsg));

        // 各日志类别独立轮转
        for (int i = 0; i < 10; ++i)
            svc.logComm(LogService::Level::Warning, longMsg);
        QVERIFY(QFileInfo(dirPath + QLatin1Char('/') + QStringLiteral("communication.log")).exists());
        QVERIFY(QFileInfo(dirPath + QLatin1Char('/') + QStringLiteral("communication.1.log")).exists());
    }

    void cleanup_removes_expired_files_only()
    {
        const QString dirPath = m_dir.path();
        {
            LogService svc(dirPath);
            svc.logApp(LogService::Level::Info, QStringLiteral("recent"));
        }

        // 制造一个 40 天前的过期文件
        const QString oldFile = dirPath + QLatin1Char('/') + QStringLiteral("old.log");
        {
            QFile f(oldFile);
            f.open(QIODevice::WriteOnly);
            f.write("old");
            f.close();
        }
        {
            QFile fOld(oldFile);
            // 注：setFileTime 要求句柄已打开（Qt 6.4 Linux 行为）
            QVERIFY(fOld.open(QIODevice::ReadWrite));
            QVERIFY(fOld.setFileTime(QDateTime::currentDateTime().addDays(-40),
                                     QFileDevice::FileModificationTime));
            fOld.close();
        }

        LogService svc(dirPath);
        svc.setRetentionDays(30);
        svc.cleanupOldLogs();

        QVERIFY(!QFile::exists(oldFile));                                    // 过期文件被删除
        QVERIFY(QFile::exists(dirPath + QLatin1Char('/') + QStringLiteral("app.log")));  // 新文件保留
    }
};

QTEST_MAIN(LogServiceTest)
#include "tst_LogService.moc"
