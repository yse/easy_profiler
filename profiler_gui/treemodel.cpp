#include "treeitem.h"
#include "treemodel.h"
#include "profiler/profiler.h"
#include <QStringList>
#include <QDataStream>
#include <QDebug>

TreeModel::TreeModel(const QByteArray &data, QObject *parent)
        : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Name" << "Duration ms" << "Percent" <<"thread id";
    m_rootItem = new TreeItem(rootData);
    setupModelData(data, m_rootItem);
}

TreeModel::~TreeModel()
{
    delete m_rootItem;
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
        return m_rootItem->columnCount();
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    return item->data(index.column());
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->data(section);

    return QVariant();
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent)
const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}



void TreeModel::setupModelData(const QByteArray &lines, TreeItem *parent)
{
    QList<TreeItem*> parents;
    QList<int> indentations;

    indentations << 0;

    typedef std::map<profiler::timestamp_t, profiler::SerializedBlock> blocks_map_t;
    typedef std::map<size_t, blocks_map_t> thread_map_t;
    thread_map_t blocksList;
    QByteArray array(lines);
    QDataStream io(&array,QIODevice::ReadOnly);




    while(!io.atEnd()){
        uint16_t sz = 0;
        io.readRawData((char*)&sz,sizeof(sz));
        char* data = new char[sz];
        io.readRawData(data,sz);
        profiler::BaseBlockData* baseData = (profiler::BaseBlockData*)data;
        blocksList[baseData->getThreadId()].emplace(
                    baseData->getBegin(),
                    /*std::move(*/profiler::SerializedBlock(sz, data))/*)*/;
    }




    for (auto& threads_list : blocksList){

        std::list<const profiler::BaseBlockData *> parents_blocks;
        parents.clear();
        parents << parent;

        double rootDuration = 0.0;

        for (auto& i : threads_list.second){
            QList<QVariant> columnData;
            double percent = 0.0;

            const profiler::BaseBlockData * _block = i.second.block();

            profiler::timestamp_t _end =  _block->getEnd();
            profiler::timestamp_t _begin =  _block->getBegin();

            bool is_root = false;
            double duration = _block->duration();
            if(parents_blocks.empty()){
                parents_blocks.push_back(_block);
                parents << parents.last();
                is_root = true;
                rootDuration =duration;
            }else{

                auto& last_block_in_stack = parents_blocks.back();
                auto last_block_end = last_block_in_stack->getEnd();

                if(_begin >= last_block_end){
                    parents_blocks.pop_back();
                    parents.pop_back();

                    for(std::list<const profiler::BaseBlockData *>::reverse_iterator it = parents_blocks.rbegin(); it != parents_blocks.rend();){
                        last_block_end = (*it)->getEnd();

                        if(_end <= last_block_end){
                            break;
                        }else{
                            parents_blocks.erase( std::next(it).base() );
                            parents.pop_back();
                        }
                    }
                }
            }
            if (!is_root){
                parents_blocks.push_back(_block);
                if(parents.size() > 1){
                    parents << parents.last()->child(parents.last()->childCount()-1);
                }else{
                    parents << parents.last();
                    is_root = true;
                }

            }
            if(is_root){
                percent = 100.0;
                rootDuration =duration;
            }else{
                percent = duration*100.0/rootDuration;
            }

            columnData << i.second.getBlockName();
            columnData << QVariant::fromValue(duration/1000000.0);

            columnData << percent;

            columnData << QVariant::fromValue(_block->getThreadId());

            if(_block->getType() == profiler::BLOCK_TYPE_BLOCK){
                parents.last()->appendChild(new TreeItem(columnData, parents.last()));
            }else{
                //parent->appendChild(new TreeItem(columnData, parent));
                parents.last()->appendChild(new TreeItem(columnData, parent));

            }

        }

    }



    return;
/*
    while (number < lines.count()) {
        int position = 0;
        while (position < lines[number].length()) {
            if (lines[number].at(position) != ' ')
                break;
            position++;
        }

        QString lineData = lines[number].mid(position).trimmed();

        if (!lineData.isEmpty()) {
            // Read the column data from the rest of the line.
            QStringList columnStrings = lineData.split("\t", QString::SkipEmptyParts);
            QList<QVariant> columnData;
            for (int column = 0; column < columnStrings.count(); ++column)
                columnData << columnStrings[column];

            if (position > indentations.last()) {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.

                if (parents.last()->childCount() > 0) {
                    parents << parents.last()->child(parents.last()->childCount()-1);
                    indentations << position;
                }
            } else {
                while (position < indentations.last() && parents.count() > 0) {
                    parents.pop_back();
                    indentations.pop_back();
                }
            }

            // Append a new item to the current parent's list of children.
            parents.last()->appendChild(new TreeItem(columnData, parents.last()));
        }

        ++number;
    }
    */
}
