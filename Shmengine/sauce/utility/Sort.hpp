#pragma once

#include "Defines.hpp"

template<typename T>
static SHMINLINE void swap_elements(T* e1, T* e2)
{
	T tmp = *e1;
	*e1 = *e2;
	*e2 = tmp;
}

template<typename T>
void quick_sort(T* arr, int32 low_index, int32 high_index, bool32 asc = true)
{
	if (low_index >= high_index || high_index <= low_index)
		return;

	// partition
	const T& pivot = arr[high_index];
	int32 i = low_index;

	for (int32 j = low_index; j < high_index; j++)
	{
		if (asc)
		{
			if (arr[j] <= pivot)
			{
				swap_elements(&arr[i], &arr[j]);
				i++;
			}
		}
		else
		{
			if (arr[j] >= pivot)
			{
				swap_elements(&arr[i], &arr[j]);
				i++;
			}
		}
		
	}

	swap_elements(&arr[i], &arr[high_index]);
	int32 partition_index = i;
	//

	quick_sort(arr, low_index, partition_index - 1, asc);
	quick_sort(arr, partition_index + 1, high_index, asc);
}
