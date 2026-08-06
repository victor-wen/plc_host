#include <QTest>
#include <QModbusDataUnit>

#include "domain/Tag.h"
#include "modbus/PollPlanner.h"

namespace {

Tag makeTag(int id, RegisterType type, int address, int intervalMs = 500,
            DataType dataType = DataType::UInt16)
{
    Tag t;
    t.id = id;
    t.registerType = type;
    t.address = address;
    t.pollIntervalMs = intervalMs;
    t.dataType = dataType;
    return t;
}

} // namespace

class PollPlannerTest : public QObject {
    Q_OBJECT
private slots:
    void singleTag_createsOneGroup()
    {
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 100)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].registerType, QModbusDataUnit::HoldingRegisters);
        QCOMPARE(groups[0].startAddress, 100);
        QCOMPARE(groups[0].count, 1);
        QCOMPARE(groups[0].intervalMs, 500);
        QCOMPARE(groups[0].tagIds, QVector<int>{1});
    }

    void twoContiguousAddresses_mergedIntoOneBlock()
    {
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 10),
             makeTag(2, RegisterType::HoldingRegister, 11)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].startAddress, 10);
        QCOMPARE(groups[0].count, 2);
        QCOMPARE(groups[0].tagIds, QVector<int>({1, 2}));
    }

    void gapOfFiveRegisters_merged()
    {
        // tag at 0 (regs 0) and tag at 6 (reg 6): gap = 6 - 0 - 1 = 5 <= 5
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 0),
             makeTag(2, RegisterType::HoldingRegister, 6)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].startAddress, 0);
        QCOMPARE(groups[0].count, 7); // block covers 0..6 including the gap
    }

    void gapGreaterThanFive_splitsIntoTwoBlocks()
    {
        // tag at 0 and tag at 7: gap = 7 - 0 - 1 = 6 > 5
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 0),
             makeTag(2, RegisterType::HoldingRegister, 7)});

        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].startAddress, 0);
        QCOMPARE(groups[0].count, 1);
        QCOMPARE(groups[1].startAddress, 7);
        QCOMPARE(groups[1].count, 1);
    }

    void differentRegisterTypes_differentGroups()
    {
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 0),
             makeTag(2, RegisterType::Coil, 0)});

        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].registerType, QModbusDataUnit::Coils);
        QCOMPARE(groups[1].registerType, QModbusDataUnit::HoldingRegisters);
    }

    void differentPollIntervalMs_differentGroups()
    {
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 0, 200),
             makeTag(2, RegisterType::HoldingRegister, 1, 500)});

        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].intervalMs, 200);
        QCOMPARE(groups[1].intervalMs, 500);
    }

    void moreThan125Registers_splits()
    {
        QVector<Tag> tags;
        for (int i = 0; i < 130; ++i)
            tags.append(makeTag(i + 1, RegisterType::HoldingRegister, i));

        PollPlanner planner;
        const auto groups = planner.buildGroups(tags);

        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].count, 125);
        QCOMPARE(groups[0].startAddress, 0);
        QCOMPARE(groups[1].count, 5);
        QCOMPARE(groups[1].startAddress, 125);
        QCOMPARE(groups[0].tagIds.size(), 125);
        QCOMPARE(groups[1].tagIds.size(), 5);
    }

    void emptyTagList_returnsEmpty()
    {
        PollPlanner planner;
        QVERIFY(planner.buildGroups({}).isEmpty());
    }

    void multiRegisterTag_extendsBlockSpan()
    {
        // Int32 at 0 covers regs 0..1; UInt16 at 2: gap = 2 - 1 - 1 = 0 -> merged
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 0, 500, DataType::Int32),
             makeTag(2, RegisterType::HoldingRegister, 2)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].startAddress, 0);
        QCOMPARE(groups[0].count, 3);
        QCOMPARE(groups[0].tagIds, QVector<int>({1, 2}));
    }

    void overlappingTags_mergedIntoSingleBlock()
    {
        // Int32@10 covers regs 10..11; UInt16@10 covers reg 10 (重叠).
        // count 必须取 max(tagEnd) 之后的整体跨度，即 11 - 10 + 1 = 2。
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 10, 500, DataType::Int32),
             makeTag(2, RegisterType::HoldingRegister, 10)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].startAddress, 10);
        QCOMPARE(groups[0].count, 2);
        QCOMPARE(groups[0].tagIds, QVector<int>({1, 2}));
    }

    void overlappingTag_thenTailExtension_mergedIntoSingleBlock()
    {
        // Int32@10 (10..11) + UInt16@10 (10) + UInt16@17 (17):
        // 重叠后 currentEnd 保持 11，再与 17 合并 gap = 17-11-1 = 5 <= 5，
        // 最终一块覆盖 10..17，count = 17 - 10 + 1 = 8。
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(1, RegisterType::HoldingRegister, 10, 500, DataType::Int32),
             makeTag(2, RegisterType::HoldingRegister, 10),
             makeTag(3, RegisterType::HoldingRegister, 17)});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].startAddress, 10);
        QCOMPARE(groups[0].count, 8);
        QCOMPARE(groups[0].tagIds, QVector<int>({1, 2, 3}));
    }

    void unsortedTags_sortedByAddress()
    {
        PollPlanner planner;
        const auto groups = planner.buildGroups(
            {makeTag(3, RegisterType::HoldingRegister, 30),
             makeTag(1, RegisterType::HoldingRegister, 10),
             makeTag(2, RegisterType::HoldingRegister, 11)});

        // 10 and 11 merge; gap 30 - 11 - 1 = 18 > 5 -> separate block
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].startAddress, 10);
        QCOMPARE(groups[0].tagIds, QVector<int>({1, 2}));
        QCOMPARE(groups[1].startAddress, 30);
        QCOMPARE(groups[1].tagIds, QVector<int>({3}));
    }
};

QTEST_MAIN(PollPlannerTest)
#include "tst_PollPlanner.moc"
