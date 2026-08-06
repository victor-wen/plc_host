#include <QTest>
#include "domain/Tag.h"
#include "domain/TagValue.h"

class DomainModelsTest : public QObject {
    Q_OBJECT
private slots:
    void tag_registerCount_UInt16_returns_1()
    {
        Tag tag;
        tag.dataType = DataType::UInt16;
        QCOMPARE(tag.registerCount(), 1);
    }

    void tag_registerCount_Int32_returns_2()
    {
        Tag tag;
        tag.dataType = DataType::Int32;
        QCOMPARE(tag.registerCount(), 2);
    }

    void tag_registerCount_Float32_returns_2()
    {
        Tag tag;
        tag.dataType = DataType::Float32;
        QCOMPARE(tag.registerCount(), 2);
    }

    void tag_registerCount_Bool_returns_1()
    {
        Tag tag;
        tag.dataType = DataType::Bool;
        QCOMPARE(tag.registerCount(), 1);
    }

    void tag_modbusAddress_returns_address()
    {
        Tag tag;
        tag.address = 100;
        QCOMPARE(tag.modbusAddress(), 100);
    }

    void tagValue_default_quality_is_Disconnected()
    {
        TagValue tv;
        QCOMPARE(tv.quality, Quality::Disconnected);
    }

    void tagValue_default_tagId_is_negative()
    {
        TagValue tv;
        QCOMPARE(tv.tagId, -1);
    }
};

QTEST_MAIN(DomainModelsTest)
#include "tst_DomainModels.moc"
