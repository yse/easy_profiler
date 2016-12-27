/************************************************************************
* file name         : easy_graphics_item.h
* ----------------- :
* creation time     : 2016/09/15
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of EasyGraphicsItem - an item
*                   : used to draw profiler blocks on graphics scene.
* ----------------- :
* change log        : * 2016/09/15 Victor Zarubkin: moved sources from blocks_graphics_view.h/.cpp
*                   :
*                   : *
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   :
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   :
*                   :
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#ifndef EASY__GRAPHICS_ITEM__H_
#define EASY__GRAPHICS_ITEM__H_

#include <stdlib.h>
#include <QGraphicsItem>
#include <QRectF>
#include <QString>
#include "easy/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class EasyGraphicsView;

class EasyGraphicsItem : public QGraphicsItem
{
    typedef ::profiler_gui::EasyItems       Children;
    typedef ::std::vector<uint32_t>      DrawIndexes;
    typedef ::std::vector<qreal>         RightBounds;
    typedef ::std::vector<Children>        Sublevels;

    DrawIndexes               m_levelsIndexes; ///< Indexes of first item on each level from which we must start painting
    RightBounds                 m_rightBounds; ///< 
    Sublevels                        m_levels; ///< Arrays of items for each level

    QRectF                     m_boundingRect; ///< boundingRect (see QGraphicsItem)
    QString                      m_threadName; ///< 
    const ::profiler::BlocksTreeRoot* m_pRoot; ///< Pointer to the root profiler block (thread block). Used by ProfTreeWidget to restore hierarchy.
    uint8_t                           m_index; ///< This item's index in the list of items of EasyGraphicsView

public:

    explicit EasyGraphicsItem(uint8_t _index, const::profiler::BlocksTreeRoot& _root);
    virtual ~EasyGraphicsItem();

    // Public virtual methods

    QRectF boundingRect() const override;

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    void validateName();

    const ::profiler::BlocksTreeRoot* root() const;
    const QString& threadName() const;

    QRect getRect() const;

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    ::profiler::thread_id_t threadId() const;

    ///< Returns number of levels
    uint8_t levels() const;

    float levelY(uint8_t _level) const;

    /** \brief Sets number of levels.
    
    \note Must be set before doing anything else.
    
    \param _levels Desired number of levels */
    void setLevels(uint8_t _levels);

    /** \brief Reserves memory for desired number of items on specified level.
    
    \param _level Index of the level
    \param _items Desired number of items on this level */
    void reserve(uint8_t _level, unsigned int _items);

    /**\brief Returns reference to the array of items of specified level.
    
    \param _level Index of the level */
    const Children& items(uint8_t _level) const;

    /**\brief Returns reference to the item with required index on specified level.
    
    \param _level Index of the level
    \param _index Index of required item */
    const ::profiler_gui::EasyBlockItem& getItem(uint8_t _level, unsigned int _index) const;

    /**\brief Returns reference to the item with required index on specified level.

    \param _level Index of the level
    \param _index Index of required item */
    ::profiler_gui::EasyBlockItem& getItem(uint8_t _level, unsigned int _index);

    /** \brief Adds new item to required level.
    
    \param _level Index of the level
    
    \retval Index of the new created item */
    unsigned int addItem(uint8_t _level);

    /** \brief Finds top-level blocks which are intersects with required selection zone.

    \note Found blocks will be added into the array of selected blocks.
    
    \param _left Left bound of the selection zone
    \param _right Right bound of the selection zone
    \param _blocks Reference to the array of selected blocks */
    void getBlocks(qreal _left, qreal _right, ::profiler_gui::TreeBlocks& _blocks) const;

    const ::profiler_gui::EasyBlock* intersect(const QPointF& _pos, ::profiler::block_index_t& _blockIndex) const;
    const ::profiler_gui::EasyBlock* intersectEvent(const QPointF& _pos) const;

private:

    ///< Returns pointer to the EasyGraphicsView widget.
    const EasyGraphicsView* view() const;

#ifdef EASY_GRAPHICS_ITEM_RECURSIVE_PAINT
    void paintChildren(const float _minWidth, const int _narrowSizeHalf, const uint8_t _levelsNumber, QPainter* _painter, struct EasyPainterInformation& p, ::profiler_gui::EasyBlockItem& _item, const ::profiler_gui::EasyBlock& _itemBlock, RightBounds& _rightBounds, uint8_t _level, int8_t _mode);
#endif

public:

    // Public inline methods

    ///< Returns this item's index in the list of graphics items of EasyGraphicsView
    inline uint8_t index() const
    {
        return m_index;
    }

}; // END of class EasyGraphicsItem.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY__GRAPHICS_ITEM__H_
