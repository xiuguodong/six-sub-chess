﻿#include "Checkerboard.h"

#include <random>
#include "VisibleRect.h"
using namespace cocos2d;


const int kNormalChessPieceZOrder = 1;
const int kSelectedChessPieceZOrder = 2;
const Vec2 kInvalidCheckerboardPos(-1.0f, -1.0f);


// 七色
static std::vector<Color4B> g_seven_colors =
{
	Color4B(255, 0, 0, 255),
	Color4B(255, 165, 0, 255),
	Color4B(255, 255, 0, 255),
	Color4B(0, 128, 0, 255),
	Color4B(0, 255, 255, 255),
	Color4B(0, 0, 255, 255),
	Color4B(79, 47, 79, 255)
};

Checkerboard::Checkerboard()
	: action_lock_(false)
	, selected_chesspiece_(nullptr)
{
	for (size_t i = 0; i < chesspiece_sprite_.size(); ++i)
	{
		chesspiece_sprite_[i] = nullptr;
	}
}

Checkerboard::~Checkerboard()
{

}

// 获取棋子开始位置
Vec2 Checkerboard::get_chesspiece_start_pos() const
{
	Vec2 start_pos;
	start_pos.x = VisibleRect::center().x - (kChessPieceWidth * LogicHandle::kCheckerboardColNum + kInterval * (LogicHandle::kCheckerboardColNum - 1)) / 2;
	start_pos.y = VisibleRect::center().y - (kChessPieceHeight * LogicHandle::kCheckerboardRowNum + kInterval * (LogicHandle::kCheckerboardRowNum - 1)) / 2;
	return start_pos;
}

bool Checkerboard::init()
{
	if (!Layer::init())
	{
		return false;
	}

	// 初始化棋盘
	Vec2 start_pos = get_chesspiece_start_pos();
	for (size_t i = 0; i < color_floor_.size(); ++i)
	{
		int row = i / LogicHandle::kCheckerboardRowNum;
		int col = i % LogicHandle::kCheckerboardRowNum;
		auto layer = LayerColor::create();
		layer->setContentSize(Size(kChessPieceWidth, kChessPieceHeight));
		layer->setPosition(start_pos + Vec2(col * kChessPieceWidth + col * kInterval, row * kChessPieceHeight + row * kInterval));
		color_floor_[i] = layer;
		addChild(layer);
	}

	// 刷新棋盘
	refresh_checkerboard();

	// 注册棋盘事件
	LogicHandle::instance()->add_event_update_notice(std::bind(&Checkerboard::update_action, this));

	// 开启触摸
	auto listener = EventListenerTouchOneByOne::create();
	listener->setSwallowTouches(true);

	listener->onTouchBegan = CC_CALLBACK_2(Checkerboard::onTouchBegan, this);
	listener->onTouchMoved = CC_CALLBACK_2(Checkerboard::onTouchMoved, this);
	listener->onTouchEnded = CC_CALLBACK_2(Checkerboard::onTouchEnded, this);
	listener->onTouchCancelled = CC_CALLBACK_2(Checkerboard::onTouchCancelled, this);

	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

	return true;
}

// 刷新棋盘
void Checkerboard::refresh_checkerboard()
{
	// 生成地板
	std::default_random_engine generator(time(nullptr));
	std::uniform_int_distribution<int> dis(g_seven_colors.size() - 1, g_seven_colors.size() - 1);
	const Color4B color = g_seven_colors[dis(generator)];

	for (size_t i = 0; i < color_floor_.size(); ++i)
	{
		int index = (i % LogicHandle::kCheckerboardRowNum) % 2;
		if (color_floor_[i] != nullptr)
		{
			color_floor_[i]->initWithColor(color, kChessPieceWidth, kChessPieceHeight);
		}
	}

	// 清理棋子精灵
	for (size_t i = 0; i < chesspiece_sprite_.size(); ++i)
	{
		if (chesspiece_sprite_[i] != nullptr)
		{
			free_sprite_.push_back(chesspiece_sprite_[i]);
			chesspiece_sprite_[i]->setVisible(false);
			chesspiece_sprite_[i]->stopAllActions();
			chesspiece_sprite_[i] = nullptr;
		}
	}

	// 绘制棋子精灵
	Vec2 start_pos = get_chesspiece_start_pos();
	LogicHandle::instance()->visit_checkerboard([&](const Vec2 &pos, int value)
	{
		if (value != 0)
		{
			Vec2 new_pos(start_pos + Vec2(pos.x * kChessPieceWidth + pos.x * kInterval + kChessPieceWidth / 2,
				pos.y * kChessPieceHeight + pos.y * kInterval + kChessPieceHeight / 2));

			Sprite *chess_piece = nullptr;
			if (free_sprite_.empty())
			{
				chess_piece  = Sprite::create(value == 1 ? "whiteplay.png" : "blackplay.png");
				addChild(chess_piece);
			}
			else
			{
				chess_piece = free_sprite_.back();
				free_sprite_.pop_back();
				Texture2D *texture = Director::getInstance()->getTextureCache()->addImage(value == 1 ? "whiteplay.png" : "blackplay.png");
				chess_piece->setTexture(texture);
				chess_piece->setTextureRect(Rect(0, 0, texture->getContentSize().width, texture->getContentSize().height));
				chess_piece->setVisible(true);
			}
			chess_piece->setPosition(new_pos);
			chess_piece->setLocalZOrder(kNormalChessPieceZOrder);
			int index = pos.y * LogicHandle::kCheckerboardColNum + pos.x;
			assert(chesspiece_sprite_[index] == nullptr);
			chesspiece_sprite_[index] = chess_piece;
		}
	});
}

// 棋盘坐标转换到世界坐标
Vec2 Checkerboard::convert_to_world_space(const Vec2 &pos) const
{
	Vec2 start_pos = get_chesspiece_start_pos();
	return start_pos +
		Vec2(pos.x * kChessPieceWidth + pos.x * kInterval + kChessPieceWidth / 2,
		pos.y * kChessPieceHeight + pos.y * kInterval + kChessPieceHeight / 2);
}

// 世界坐标转换棋盘坐标
Vec2 Checkerboard::convert_to_checkerboard_space(const Vec2 &pos) const
{
	Vec2 start_pos = get_chesspiece_start_pos();
	int col = (pos.x - start_pos.x - kInterval) / kChessPieceWidth;
	int row = (pos.y - start_pos.y - kInterval) / kChessPieceHeight;
	col = col < 0 ? 0 : col >= LogicHandle::kCheckerboardColNum ? LogicHandle::kCheckerboardColNum - 1 : col;
	row = row < 0 ? 0 : row >= LogicHandle::kCheckerboardRowNum ? LogicHandle::kCheckerboardRowNum - 1 : row;
	return Vec2(col , row);
}

bool Checkerboard::onTouchBegan(Touch *touch, Event *unused_event)
{
	if (!action_lock_)
	{
		auto chesspiece = get_chesspiece_sprite(convert_to_checkerboard_space(touch->getLocation()));
		if (chesspiece != nullptr)
		{
			selected_chesspiece_ = chesspiece;
			selected_chesspiece_->setLocalZOrder(kSelectedChessPieceZOrder);
			touch_begin_pos_ = touch->getLocation();
			return true;
		}
	}
	return false;
}

void Checkerboard::onTouchMoved(Touch *touch, Event *unused_event)
{
	// 移动棋子
	assert(selected_chesspiece_ != nullptr);
	if (selected_chesspiece_ != nullptr)
	{
		selected_chesspiece_->setPosition(touch->getLocation());
	}
}

void Checkerboard::onTouchEnded(Touch *touch, Event *unused_event)
{
	if (selected_chesspiece_ != nullptr)
	{
		auto source = convert_to_checkerboard_space(touch_begin_pos_);
		auto target = convert_to_checkerboard_space(touch->getLocation());

		if (LogicHandle::instance()->is_adjacent(source, target) &&
			!LogicHandle::instance()->is_valid_chess_piece(target))
		{
			selected_chesspiece_->setPosition(convert_to_world_space(target));
			LogicHandle::instance()->move_chess_piece(source, target);

			int s_index = source.y * LogicHandle::kCheckerboardColNum + source.x;
			int t_index = target.y * LogicHandle::kCheckerboardColNum + target.x;
			std::swap(chesspiece_sprite_[s_index], chesspiece_sprite_[t_index]);
		}
		else
		{
			selected_chesspiece_->setPosition(convert_to_world_space(source));
		}

		selected_chesspiece_->setLocalZOrder(kNormalChessPieceZOrder);
		selected_chesspiece_ = nullptr;
	}	
}

void Checkerboard::onTouchCancelled(Touch *touch, Event *unused_event)
{
	onTouchEnded(touch, unused_event);
}

// 获取棋子精灵
Sprite* Checkerboard::get_chesspiece_sprite(const Vec2 &pos)
{
	int index = pos.y * LogicHandle::kCheckerboardColNum + pos.x;
	return chesspiece_sprite_[index];
}

// 移动棋子
void Checkerboard::on_move_chesspiece(const Vec2 &source, const Vec2 &target)
{
	Sprite *chess_piece = get_chesspiece_sprite(source);
	assert(chess_piece != nullptr);
	if (chess_piece != nullptr)
	{
		Vec2 world_pos = convert_to_world_space(target);
		chess_piece->runAction(Sequence::create(
			MoveTo::create(0.1f, world_pos),
			CallFunc::create([=]()
		{
			int s_index = source.y * LogicHandle::kCheckerboardColNum + source.x;
			int t_index = target.y * LogicHandle::kCheckerboardColNum + target.x;
			std::swap(chesspiece_sprite_[s_index], chesspiece_sprite_[t_index]);
		}),
			CallFunc::create(std::bind(&Checkerboard::finished_action, this)),
			nullptr));
	}
}

// 吃掉棋子
void Checkerboard::on_kill_chesspiece(const Vec2 &source, const Vec2 &target)
{
	Sprite *chess_piece = get_chesspiece_sprite(target);
	assert(chess_piece != nullptr);
	if (chess_piece != nullptr)
	{
		chess_piece->setVisible(false);
		chess_piece->runAction(Sequence::create(
			CallFunc::create([=]()
		{
			int index = target.y * LogicHandle::kCheckerboardColNum + target.x;
			free_sprite_.push_back(chesspiece_sprite_[index]);
			chesspiece_sprite_[index] = nullptr;
		}),
			CallFunc::create(std::bind(&Checkerboard::finished_action, this)),
			nullptr));
	}
}

// 更新动作
void Checkerboard::update_action()
{
	perform_action();
}

// 执行动作
void Checkerboard::perform_action()
{
	if (!action_lock_)
	{
		LogicHandle::EventDetails action = LogicHandle::instance()->take_event_info();
		if (action.type != LogicHandle::EventType::NONE)
		{
			// 移动，自己的移动操作实时处理，不通过逻辑处理器进行
			if (action.type == LogicHandle::EventType::MOVED)
			{
				if (action.chesspiece != 1)
				{
					action_lock_ = true;
					on_move_chesspiece(action.source, action.target);
				}
				else
				{
					perform_action();
					return;
				}
			}
			// 吃子
			else if (action.type == LogicHandle::EventType::KILLED)
			{
				action_lock_ = true;
				on_kill_chesspiece(action.source, action.target);
			}
		}
	}
}

// 完成动作
void Checkerboard::finished_action()
{
	action_lock_ = false;
	perform_action();
}