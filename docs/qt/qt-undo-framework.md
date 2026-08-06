# Qt 撤销框架

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/qundostack.html>、<https://doc.qt.io/qt-6.8/qundocommand.html>、<https://doc.qt.io/qt-6.8/qundo.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 设计原则

Qt Undo Framework 是 Command 模式：所有可撤销的文档/看板变更都包装成 `QUndoCommand`，按顺序放入 `QUndoStack`。只要模型变更都经过 command，就能通过调用 `undo()` 逆向回滚，通过 `redo()` 正向重放。

模块属于 Qt GUI：

```cpp
find_package(Qt6 REQUIRED COMPONENTS Gui)
target_link_libraries(mytarget PRIVATE Qt6::Gui)
```

## QUndoStack

`QUndoStack` 继承 `QObject`，拥有压入的命令。构造函数和常用 API：

```cpp
explicit QUndoStack(QObject *parent = nullptr);

void push(QUndoCommand *cmd);
void undo();
void redo();
bool canUndo() const;
bool canRedo() const;
bool isClean() const;
void setClean();
void resetClean();
int count() const;
int index() const;
int cleanIndex() const;
void setIndex(int idx);
void clear();

int undoLimit() const;
void setUndoLimit(int limit);
QString undoText() const;
QString redoText() const;
QAction *createUndoAction(QObject *parent,
                          const QString &prefix = QString()) const;
QAction *createRedoAction(QObject *parent,
                          const QString &prefix = QString()) const;
```

`push()` 会执行新命令的 `redo()`，随后取得 stack 所有权。若此前已经撤销过命令，再 push 会删除当前 index 之后的 redo 分支。通过 `createUndoAction()`/`createRedoAction()` 可得到自动更新文字和 enabled 状态的 QAction。

### clean 状态和信号

保存到磁盘后调用 `setClean()`。当 stack 离开或重新回到 clean index 时，会发出：

```cpp
void cleanChanged(bool clean);
void canUndoChanged(bool canUndo);
void canRedoChanged(bool canRedo);
void indexChanged(int idx);
void undoTextChanged(const QString &undoText);
void redoTextChanged(const QString &redoText);
```

`cleanChanged` 适合驱动窗口标题的未保存标记。`resetClean()` 将 clean index 设为 `-1`，用于恢复备份或“永远不再回到原保存状态”的场景。`clear()` 删除所有命令并把 stack 置为 clean，但不会对文档执行 undo/redo，文档当前状态保持不变。

### 栈大小

`undoLimit` 是栈上最多保留的 command 数量，默认 `0` 表示不限制。超过上限时从栈底删除旧命令；带子命令的宏整体算一个 command。Qt 规定只能在 stack 为空时设置 `setUndoLimit()`；非空时调用会警告并不生效。

## QUndoCommand

`QUndoCommand` 是所有命令的基类，命令本身可有父命令和子命令：

```cpp
explicit QUndoCommand(QUndoCommand *parent = nullptr);
explicit QUndoCommand(const QString &text,
                      QUndoCommand *parent = nullptr);
virtual ~QUndoCommand();

virtual void undo();
virtual void redo();
virtual int id() const;
virtual bool mergeWith(const QUndoCommand *command);

void setText(const QString &text);
QString text() const;
QString actionText() const;
int childCount() const;
const QUndoCommand *child(int index) const;
void setObsolete(bool obsolete);
bool isObsolete() const;
```

派生类必须实现 `undo()` 和 `redo()`。基类实现会按顺序对 child 执行 `redo()`，按逆序对 child 执行 `undo()`。`setText()` 的短文本显示在 `QUndoView` 和 undo/redo QAction 中；若需要区分 view 文本和 action 文本，可按 Qt 文档约定用换行分隔。

`undo()`/`redo()` 内不要调用 `QUndoStack::push()`、`undo()` 或 `redo()`；Qt 文档明确指出这样会导致未定义行为。

## mergeWith：命令压缩

默认 `id()` 返回 `-1`，表示不支持合并。要支持合并，派生类返回一个在该命令类型内唯一的非负 ID，并实现：

```cpp
int id() const override;
bool mergeWith(const QUndoCommand *command) override;
```

`QUndoStack::push()` 只会尝试把新命令和最近执行的命令合并，且两个 ID 必须相同且不为 `-1`。`mergeWith()` 返回 `true` 后，新 command 会被删除；当前 command 的一次 `redo()` 必须等价于两个 command 的 redo 合并效果，而一次 `undo()` 必须等价于按新命令、旧命令的反向顺序撤销。

典型的连续移动压缩：

```cpp
class MoveItemCommand final : public QUndoCommand
{
public:
    MoveItemCommand(ItemModel *model, int itemId,
                    QPointF oldPos, QPointF newPos,
                    QUndoCommand *parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_itemId(itemId),
          m_oldPos(oldPos), m_newPos(newPos)
    {
        setText(QStringLiteral("move item"));
    }

    void undo() override { m_model->setPosition(m_itemId, m_oldPos); }
    void redo() override { m_model->setPosition(m_itemId, m_newPos); }

    int id() const override { return 1001; }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *move = dynamic_cast<const MoveItemCommand *>(other);
        if (!move || move->m_itemId != m_itemId)
            return false;
        m_newPos = move->m_newPos;
        return true;
    }

private:
    ItemModel *m_model;
    int m_itemId;
    QPointF m_oldPos;
    QPointF m_newPos;
};
```

命令构造时保存旧状态和新状态，`redo()` 应用新状态，`undo()` 恢复旧状态；不要在 `mergeWith()` 中再次修改模型。

## 宏命令

宏命令把多个独立操作合并成一次用户可见的 undo/redo。可以显式建立父 command：

```cpp
auto *macro = new QUndoCommand;
macro->setText(QStringLiteral("align selected items"));
new MoveItemCommand(model, firstId, oldFirst, newFirst, macro);
new MoveItemCommand(model, secondId, oldSecond, newSecond, macro);
stack->push(macro);
```

也可以使用 stack 的便利 API：

```cpp
stack->beginMacro(QStringLiteral("align selected items"));
stack->push(new MoveItemCommand(model, firstId, oldFirst, newFirst));
stack->push(new MoveItemCommand(model, secondId, oldSecond, newSecond));
stack->endMacro();
```

`beginMacro()` 和 `endMacro()` 必须成对，可嵌套。最外层宏结束前 stack 处于禁用状态：`canUndo()`/`canRedo()` 返回 false，undo/redo 不生效，相关状态信号不会逐个发出；最外层 `endMacro()` 后才按整个宏发出一次状态变化。宏中的 child 仍分别保留，便于维护和审计。

## 看板项目用法

- 拖动、缩放、删除、属性编辑和多项对齐各自创建 command；批量对齐使用宏。
- 构造 command 时复制旧/新几何和样式，不保存短生命周期的 UI 指针或临时引用。
- `redo()`/`undo()` 只修改 UI 线程模型，并让 Graphics View 在同一线程刷新。
- 保存成功后 `setClean()`；加载新页面或丢弃草稿时 `clear()`，不要用 `clear()` 代替撤销。

## 线程安全约束

`QUndoStack`、`QUndoCommand` 以及它们操作的看板模型在 UI 主线程使用。它们不是线程安全的共享队列。后台线程不能直接 push command、调用 `undo()`/`redo()` 或修改 command 所持有的 UI 模型。

后台结果通过信号/槽（必要时 `Qt::QueuedConnection`）或 `QMetaObject::invokeMethod()` 传给 UI 线程，再由 UI 线程创建并 push command。QObject 的父子关系、销毁和事件处理遵循对象线程亲和性。
