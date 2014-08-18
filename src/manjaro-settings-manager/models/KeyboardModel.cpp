#include "KeyboardModel.h"
#include "models/KeyboardItem.h"

#include <QtCore/QFile>
#include <QtCore/QProcess>

#include <QDebug>

KeyboardModel::KeyboardModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem_ = new KeyboardItem(QString("key"), QString("description"));
    init(rootItem_);
}


KeyboardModel::~KeyboardModel()
{
    delete rootItem_;
}


QVariant KeyboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    KeyboardItem *item = static_cast<KeyboardItem*>(index.internalPointer());

    if ((role == KeyRole) || ((role == Qt::DisplayRole) && (index.column() == 0))) {
        return item->key();
    } else if ((role == DescriptionRole) || ((role == Qt::DisplayRole) && (index.column() == 1))){
        return item->description();
    }

    return QVariant();
}


Qt::ItemFlags KeyboardModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


QVariant KeyboardModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) {
            return rootItem_->key();
        } else if (section == 1) {
            return rootItem_->description();
        }
    }

    return QVariant();
}


QModelIndex KeyboardModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    KeyboardItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem_;
    } else {
        parentItem = static_cast<KeyboardItem*>(parent.internalPointer());
    }

    KeyboardItem *childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    } else {
        return QModelIndex();
    }
}


QModelIndex KeyboardModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    KeyboardItem *childItem = static_cast<KeyboardItem*>(index.internalPointer());
    KeyboardItem *parentItem = childItem->parent();

    if (parentItem == rootItem_) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}


int KeyboardModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    KeyboardItem *parentItem;
    if (!parent.isValid()) {
        parentItem = rootItem_;
    } else {
        parentItem = static_cast<KeyboardItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}


int KeyboardModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return static_cast<KeyboardItem*>(parent.internalPointer())->columnCount();
    } else {
        return rootItem_->columnCount();
    }
}


QHash<int, QByteArray> KeyboardModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[KeyRole] = "key";
    roles[DescriptionRole] = "description";
    return roles;
}


void KeyboardModel::init(KeyboardItem *parent)
{
    const QString xkbFile("/usr/share/X11/xkb/rules/base.lst");

    QFile fh(xkbFile);
    fh.open(QIODevice::ReadOnly);

    if (!fh.isOpen()) {
        qDebug() << "X11 Keyboard layout and models definitions not found!";
    }

    /* Root item for layouts */
    KeyboardItem *layoutsRoot = new KeyboardItem("layouts", "keyboard layouts", parent);
    parent->appendChild(layoutsRoot);

    /* Get Layouts */
    KeyboardItem *currentLayout;
    bool layoutsFound = false;

    while (!fh.atEnd()) {
        QByteArray line = fh.readLine();

        /* find the layout section otherwhise continue. If the layout section is at it's end, break the loop */
        if (!layoutsFound && line.startsWith("! layout"))
            layoutsFound = true;
        else if (layoutsFound && line.startsWith ("!"))
            break;
        else if (!layoutsFound)
            continue;

        QRegExp rx;
        rx.setPattern("^\\s+(\\S+)\\s+(\\w.*)\n$");

        // insert into the layout model
        if (rx.indexIn(line) != -1) {
            QString layoutKey = rx.cap(1);
            QString layoutDescription = rx.cap(2);
            currentLayout = new KeyboardItem(layoutKey, layoutDescription, layoutsRoot);
            layoutsRoot->appendChild(currentLayout);
            currentLayout->appendChild(new KeyboardItem(QString("default"), QString(tr("Default")), currentLayout));
        }
    }


    /* Get Variants */
    bool variantsFound = false;
    /* Start from the beginning */
    fh.reset();
    while (!fh.atEnd()) {
        QByteArray line = fh.readLine();

        /* continue until we found the variant section. If found, read until the next section is found */
        if (!variantsFound && line.startsWith("! variant")) {
            variantsFound = true;
            continue;
        } else if (variantsFound && line.startsWith ("!"))
            break;
        else if (!variantsFound)
            continue;

        QRegExp rx;
        rx.setPattern("^\\s+(\\S+)\\s+(\\S+): (\\w.*)\n$");

        if (rx.indexIn(line) != -1) {
            QString variantKey = rx.cap(1);
            QString layoutKey = rx.cap(2);
            QString variantDescription = rx.cap(3);

            QModelIndexList layoutIndexList = match(index(0,0).child(0,0),
                                                    KeyRole,
                                                    layoutKey,
                                                    -1,
                                                    Qt::MatchExactly);

            if (layoutIndexList.isEmpty()) {
                currentLayout = new KeyboardItem(layoutKey, layoutKey, layoutsRoot);
                layoutsRoot->appendChild(currentLayout);
                currentLayout->appendChild(new KeyboardItem(QString("default"), QString(tr("Default")), currentLayout));
                currentLayout->appendChild(new KeyboardItem(variantKey, variantDescription, currentLayout));
            } else {
                currentLayout = static_cast<KeyboardItem*>(layoutIndexList.first().internalPointer());
                currentLayout->appendChild(new KeyboardItem(variantKey, variantDescription, currentLayout));
            }

        }
    }


    /* Root item for models */
    KeyboardItem *modelsRoot = new KeyboardItem("models", "keyboard models", parent);
    parent->appendChild(modelsRoot);

    /* Get models */
    bool modelsFound = false;
    /* Start from the beginning */
    fh.reset();
    while (!fh.atEnd()) {
        QByteArray line = fh.readLine();

        /* continue until we found the model section. If found, read until the next section is found */
        if (!modelsFound && line.startsWith("! model"))
            modelsFound = true;
        else if (modelsFound && line.startsWith ("!"))
            break;
        else if (!modelsFound)
            continue;

        QRegExp rx;
        rx.setPattern("^\\s+(\\S+)\\s+(\\w.*)\n$");

        /* insert into the model */
        if (rx.indexIn(line) != -1) {
            QString modelKey = rx.cap(1);
            QString modelDescription = rx.cap(2);
            if (modelKey == "pc105") {
                modelDescription += " - " + QString(tr("Default Keyboard Model"));
            }
            modelsRoot->appendChild(new KeyboardItem(modelKey, modelDescription, modelsRoot));
        }
    }
}


bool KeyboardModel::getCurrentKeyboardLayout(QString &layout, QString &variant, QString &model)
{
    layout.clear();
    variant.clear();
    model.clear();

    QProcess process;
    process.start("setxkbmap", QStringList() << "-print" << "-verbose" << "10");

    if (!process.waitForFinished())
        return false;

    /*
     * Example output
     * ...
     * model:      pc105,pc104
     * layout:     es,us
     * variant:    cat,euro
     * ...
     */
    QStringList list = QString(process.readAll()).split("\n", QString::SkipEmptyParts);
    for (QString line : list) {
        line = line.trimmed();
        if (line.startsWith("layout")) {
            QStringList split = line.split(":", QString::SkipEmptyParts);
            split = split.value(1).trimmed().split(",", QString::SkipEmptyParts);
            layout = split.value(0).trimmed();
        }
        if (line.startsWith("variant")) {
            QStringList split = line.split(":", QString::SkipEmptyParts);
            split = split.value(1).trimmed().split(",", QString::SkipEmptyParts);
            variant = split.value(0).trimmed();
        }
        if (line.startsWith("model")) {
            QStringList split = line.split(":", QString::SkipEmptyParts);
            split = split.value(1).trimmed().split(",", QString::SkipEmptyParts);
            model = split.value(0).trimmed();
        }
    }
    return !layout.isEmpty();
}
