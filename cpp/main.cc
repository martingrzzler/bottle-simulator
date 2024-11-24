#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <chrono>
#include <iostream>

class IObserver
{
public:
	virtual ~IObserver() {}
	virtual void OnEmptyPlaceChanged() = 0;
	virtual void OnFullPlaceChanged() = 0;
};

// Repräsentiert das Leitsteuerungssystem welches der Simulator bedient und beobachtet
// Um das System zu simulieren habe ich einen Producer und einen Consumer erstellt welche Leergut hinzufügen und Vollgut entfernen
class MasterControlSystem
{
private:
	std::atomic<bool> emptyPlaceOccupied;
	std::atomic<bool> fullPlaceOccupied;
	IObserver *observer;

public:
	MasterControlSystem() : emptyPlaceOccupied(false), fullPlaceOccupied(false), observer(nullptr) {}
	bool GetEmptyPlaceOccupied()
	{
		return emptyPlaceOccupied;
	}
	bool GetFullPlaceOccupied()
	{
		return fullPlaceOccupied;
	}
	void SetEmptyPlaceOccupied(bool emptyPlaceOccupied)
	{
		if (emptyPlaceOccupied == this->emptyPlaceOccupied)
			return;
		this->emptyPlaceOccupied = emptyPlaceOccupied;
		if (observer)
			observer->OnEmptyPlaceChanged();
	}
	void SetFullPlaceOccupied(bool fullPlaceOccupied)
	{
		if (fullPlaceOccupied == this->fullPlaceOccupied)
			return;
		this->fullPlaceOccupied = fullPlaceOccupied;
		if (observer)
			observer->OnFullPlaceChanged();
	}
	void AttachObserver(IObserver *observer)
	{
		this->observer = observer;
	}
	void SimulateEmptyBottlesProducer(int interval = 2)
	{
		while (true)
		{
			if (emptyPlaceOccupied)
				continue;
			std::this_thread::sleep_for(std::chrono::seconds(interval));
			emptyPlaceOccupied = true;
			printf("[Producer] Empty bottles delivered\n");
			if (observer)
				observer->OnEmptyPlaceChanged();
		}
	}
	void SimulateFullBottlesConsumer(int interval = 5)
	{
		while (true)
		{
			if (!fullPlaceOccupied)
				continue;
			std::this_thread::sleep_for(std::chrono::seconds(interval));
			fullPlaceOccupied = false;
			printf("[Consumer] Full bottles removed\n");
			if (observer)
				observer->OnFullPlaceChanged();
		}
	}
};

// Repräsentiert den Simulator der die Machinen koordiniert
class MachineSimulator : public IObserver
{
private:
	MasterControlSystem *mcs;
	std::mutex emptyPlaceLock;
	std::mutex fullPlaceLock;
	std::condition_variable emptyPlaceCond;
	std::condition_variable fullPlaceCond;

public:
	MachineSimulator(MasterControlSystem *mcs) : mcs(mcs)
	{
		mcs->AttachObserver(this);
	}
	void OnEmptyPlaceChanged() override
	{
		std::unique_lock<std::mutex> lock(emptyPlaceLock);
		if (mcs->GetEmptyPlaceOccupied())
		{
			emptyPlaceCond.notify_one();
		}
	}
	void OnFullPlaceChanged() override
	{
		std::unique_lock<std::mutex> lock(fullPlaceLock);
		if (!mcs->GetFullPlaceOccupied())
		{
			fullPlaceCond.notify_one(); 
		}
	}

	void SimulateMachines(int count)
	{
		for (int i = 0; i < count; i++)
		{
			std::thread([this, i](){ MachineTask(i); }).detach();
		}
	}
	
	// Eine Machine wartet auf leere Flaschen, füllt sie und wartet auf den Platz um sie abzustellen
	// Die implementierten Observer Methoden stellen sicher dass immer nur eine Machine geweckt wird,
	// wenn sie wartet auf leere Flaschen oder auf den Platz um die Flaschen abzustellen
	void MachineTask(int i)
	{
		while (true)
		{
			{
				std::unique_lock<std::mutex> emptyLock(emptyPlaceLock);
				printf("Machine %d is waiting for empty bottles\n", i);
				while (!mcs->GetEmptyPlaceOccupied())
				{
					emptyPlaceCond.wait(emptyLock);
				}
				printf("Machine %d has empty bottles\n", i);
			}
			mcs->SetEmptyPlaceOccupied(false);
			printf("Machine %d is filling bottles\n", i);
			// Simulate work
			std::this_thread::sleep_for(std::chrono::seconds(6));
			printf("Machine %d has filled the bottles\n", i);
			{
				std::unique_lock<std::mutex> fullLock(fullPlaceLock);
				printf("Machine %d is waiting for full place\n", i);
				while (mcs->GetFullPlaceOccupied())
				{
					fullPlaceCond.wait(fullLock);
				}
			}
			mcs->SetFullPlaceOccupied(true);
			printf("Machine %d has placed full bottles\n", i);
		}
	}
};

int main()
{
	MasterControlSystem mcs;
	MachineSimulator ms(&mcs);

	mcs.AttachObserver(&ms);

	std::thread producer([&mcs]()
						 { mcs.SimulateEmptyBottlesProducer(); });
	std::thread consumer([&mcs]()
						 { mcs.SimulateFullBottlesConsumer(); });

	ms.SimulateMachines(3);

	producer.join();
	consumer.join();

	return 0;
}