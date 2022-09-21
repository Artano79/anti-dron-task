#include <iostream>
#include <array>
#include <thread>
#include <mutex>
#include <list>

#include "figure.h"


using namespace antidron_test_task;

//consts
static const int turns_count = 50;	//кол-во ходов совершаемы каждой фигурой
static const int figures_count = 5;	//количество фигур
static std::mutex turn_mutex;		//мьютекс, контролирующий, что в один момент времени свое положение
									//меняет только одна фигура
									//(Уточню: доступ к объекту шахаматной доски и коллекции фигур НЕ потоко-безопасен.
									//С доступом к std::cout из разных потоков - я тоже никакой защиты не делал)


//pseudonames
using milliseconds = std::chrono::milliseconds;
using time_point = std::chrono::system_clock::time_point;


void figure_thread_func(board::fig_index index);

int main()
{
	try
	{
		//Создаем объект шахматной доски как singleton-object
		board &b = board::getInstance();
	
		//Начальные позиции фигур в формате {x,y}
		std::array<pos_t, figures_count> initial_pos{ { {1,1}, {2,1}, {3,7}, {6,2}, {1,0} } };
	
		//Добавляем фигуры на доску
		for (auto& pos : initial_pos)
			b.add_figure(pos);

		//Выводим доску на экран
		b.print();
		std::cout << std::endl;

		//Запускаем потоки по количеству фигур
		std::cout << "Waiting..." << std::endl;
		std::list<std::thread> threads;
		for (board::fig_index idx = 0; idx < figures_count; idx++)
			threads.emplace_back(figure_thread_func, idx);

		//Ожидание завершения потоков
		for (auto& pos_t : threads)
			pos_t.join();
	
		//Выводим доску на экран
		b.print();
	}
	catch (const std::exception& e)
	{
		std::cout << "unexpected exception in main thread: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cout << "Unknown exception in main thread: " << std::endl;
	}
}

void figure_thread_func(board::fig_index index)
{
	try
	{
		//получаем ссылку на доску
		board &b = board::getInstance();
		
		//Получаем ссылку на фигуру, которую "двигает" данный поток
		const figure &f = b.get_figure(index);
		
		//Пока фигура не закончила ходить
		while (f.get_turn() < turns_count)
		{
			//Каждый четный ход поток засыпает на случайную величину[200-300ms]
			if (f.get_turn() % 2 > 0)
				std::this_thread::sleep_for(milliseconds(200 + std::rand() % 300));

			//Генерируем новую позицию, в которую должна двигаться фигура
			const pos_t destination_pos = b.new_random_pos(index);
			
			//blocked_fig_idx - переменная содержит индекс фигуры, которая блокирует проход
			//Инициализируется некорректным индексом
			board::fig_index blocked_fig_idx = figures_count;
			
			//Номер хода, на котором находится фигура с индексом blocked_fig_idx
			int blocked_fig_turn;
			
			//Момент времени до коорого мы ждем, пока проход не разблокируется
			const time_point until_time = std::chrono::high_resolution_clock::now() + milliseconds(5000);
			
			
			while(true){
				
				{
					std::lock_guard lk(turn_mutex);

					//Пытаемся переместить нашу фигуру. Если проход заблокирован получаем индекс блокирующей фигуры
					//Или невалидный индекс, перемщение было успешно совершено
					blocked_fig_idx = b.try_to_move_to(index, destination_pos.x_,destination_pos.y_);
					
					//Запрашиваем номер хода, на котором находится блокирующая фигура
					if(blocked_fig_idx < figures_count)
						blocked_fig_turn = b.get_figure(blocked_fig_idx).get_turn();
				}

				//Если проход не был блокирован, переходим к следубщему ходу
				if(blocked_fig_idx >= figures_count)
					break;

				//Функция ждет, пока блокирующпя фигура не совершит ход (внутри проверяется изменение ее номера хода)
				bool wait_result = b.wait_while_figure_make_turn(blocked_fig_idx, blocked_fig_turn, until_time);
				
				//Полученный результат false, если вышло время. Переходим к следующему ходу (счетчик хода не увеличится)
				if(!wait_result)
					break;

				//Если полученный результат был положительным, зановой пытаемся перемстить фигуру 
			}
		}
	}
	catch(const std::exception& e)
	{
		std::cout << "Thread of Figure " << index << " catch exception. Message: " << e.what() << std::endl;
		return;
	}
	catch(...)
	{
		std::cout << "Thread of Figure " << index << " got unknown exception. " << std::endl;
		return;
	}
	
	std::cout << "Thread of Figure " << index << " finished..." << std::endl;
}