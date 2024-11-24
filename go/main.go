package main

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"time"
)

type Timer struct {
	callback func()
	duration time.Duration
	cancel   context.CancelFunc
}

type Sensor struct {
	emptyPlaceOccupied bool
	fullPlaceOccupied  bool
	timers             map[TimerID]Timer
}

type TimerID int

type MachineSimulator struct {
	sensor       *Sensor
	emptyBottles chan struct{}
	fullBottles  chan struct{}
}

func (s *Sensor) SetEmptyPlaceSensorOccupied(occupied bool) {
	s.emptyPlaceOccupied = occupied
}

func (s Sensor) GetEmptyPlaceSensorOccupied() bool {
	return s.emptyPlaceOccupied
}

func (s *Sensor) SetFullPlaceSensorOccupied(occupied bool) {
	s.fullPlaceOccupied = occupied
}

func (s Sensor) GetFullPlaceSensorOccupied() bool {
	return s.fullPlaceOccupied
}

func (s *Sensor) KillTimer(id TimerID) {
	if timer, ok := s.timers[id]; ok {
		timer.cancel()
	}
}

func (s *Sensor) StartTimer(d time.Duration, callback func()) TimerID {
	ctx, cancel := context.WithCancel(context.Background())
	id := TimerID(len(s.timers))
	s.timers[id] = Timer{
		callback: callback,
		duration: d,
		cancel:   cancel,
	}

	go func() {
		select {
		case <-time.After(d):
			callback()
			delete(s.timers, id)
		case <-ctx.Done():
			delete(s.timers, id)
			return
		}
	}()

	return id
}

func (s *MachineSimulator) OnEmptyPlaceSensorChanged() {
	if s.sensor.GetEmptyPlaceSensorOccupied() {
		fmt.Printf("New empty bottles arrived, sending to machines\n")
		s.emptyBottles <- struct{}{}
		fmt.Printf("Empty bottles sent to machines\n")
		s.sensor.SetEmptyPlaceSensorOccupied(false)
	}
}

func (s *MachineSimulator) OnFullPlaceSensorChanged() {
	if !s.sensor.GetFullPlaceSensorOccupied() {
		fmt.Printf("Full bottles removed, take full bottles from machines\n")
		<-s.fullBottles
		fmt.Printf("Full bottles taken from one and placed in the full place\n")
		s.sensor.SetFullPlaceSensorOccupied(true)
	}
}

func (s *MachineSimulator) Run() {
	// the three machines
	for i := 0; i < 3; i++ {
		go func() {
			for range s.emptyBottles {
				fmt.Printf("Machine %d: receives empty bottles\n", i)
				fmt.Printf("Machine %d: starts filling bottles\n", i)
				// sleep for a random time between 2 and 6 seconds
				time.Sleep(time.Duration(rand.Intn(5)+2) * time.Second)
				fmt.Printf("Machine %d: finished filling bottles\n", i)
				s.fullBottles <- struct{}{}
				fmt.Printf("Machine %d: has placed full bottles\n", i)
			}
		}()
	}
}

func main() {
	sensor := Sensor{
		emptyPlaceOccupied: false,
		fullPlaceOccupied:  false,
		timers:             make(map[TimerID]Timer),
	}

	ms := MachineSimulator{
		sensor:       &sensor,
		emptyBottles: make(chan struct{}),
		fullBottles:  make(chan struct{}),
	}

	wg := sync.WaitGroup{}

	// the delivery of empty bottles
	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			time.Sleep(2 * time.Second)
			if !sensor.GetEmptyPlaceSensorOccupied() {
				sensor.SetEmptyPlaceSensorOccupied(true)
				ms.OnEmptyPlaceSensorChanged()
			}
		}
	}()

	// the taking of full bottles
	wg.Add(1)
	go func() {
		defer wg.Done()

		// initially signaling that the full place is empty
		ms.OnFullPlaceSensorChanged()

		for {
			time.Sleep(5 * time.Second)
			if sensor.GetFullPlaceSensorOccupied() {
				sensor.SetFullPlaceSensorOccupied(false)
				ms.OnFullPlaceSensorChanged()
			}
		}
	}()

	ms.Run()
	wg.Wait()
}
