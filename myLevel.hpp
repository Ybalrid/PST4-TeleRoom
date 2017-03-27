#ifndef MY_LEVEL
#define MY_LEVEL

#include <Annwvyn.h>

//Each level you can create inherits
//from AnnAbstractLevel

class MyLevel;

class ObjectGrabber : LISTENER
{
public:
	ObjectGrabber(MyLevel* level);

	void HandControllerEvent(Annwvyn::AnnHandControllerEvent e) override;
	void MouseEvent(Annwvyn::AnnMouseEvent e) override;

private:
	MyLevel* level;

	bool lastState;
};

class MyLevel : LEVEL
{
public:
	using GameObjectVector = std::vector<std::shared_ptr<Annwvyn::AnnGameObject>>;
	MyLevel();
	void load() override;
	void runLogic() override;

	void unload() override;

	GameObjectVector& getReachables() { return reachable; }

	void grabRequested();
	void ungrab();


private:
	std::vector<std::shared_ptr<Annwvyn::AnnGameObject>> grabable;
	std::vector<std::shared_ptr<Annwvyn::AnnGameObject>> reachable;
	Annwvyn:: AnnVect3 squaredThreshold;

	std::shared_ptr<ObjectGrabber> grabber;

	std::shared_ptr<Annwvyn::AnnGameObject> grabbed;

};

#endif //MY_LEVEL