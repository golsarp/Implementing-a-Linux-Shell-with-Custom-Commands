#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SARP GOL");
MODULE_DESCRIPTION("Traverse kernel module.");
MODULE_VERSION("0.01");
int pid = 0;
char *flag;
module_param(pid,int,0);
module_param(flag,charp,0);

void DFS(struct task_struct *task){
    
    struct task_struct *next_task;
	struct list_head *list;
	list_for_each(list, &task->children) {
		next_task = list_entry(list, struct task_struct, sibling);
		printk(KERN_INFO "pid: %d pname: %s \n",next_task->pid, next_task->comm);
		DFS(next_task);
	}	
    
    
}

void BFS(struct task_struct *task){

    struct task_struct *next_task;
	struct list_head *list;
    //loop in kernel using list_for_each function 
	list_for_each(list, &task->children) {
        //get the next task
		next_task = list_entry(list, struct task_struct, sibling);
        //cal BFS recursively
        BFS(next_task);
		printk(KERN_INFO "pid: %d pname: %s \n",next_task->pid, next_task->comm);
		
	}	
}


 int __init traverse_init(void) {
 printk(KERN_INFO "Hello, World!\n");
 printk("root: %d",pid);
 struct pid *pid_struct;
	struct task_struct *task;
	
    //get task by using root pid
	pid_struct = find_get_pid(pid);
	task = pid_task(pid_struct, PIDTYPE_PID);
    if(task == NULL){
       
    }else{
        if(strcmp(flag,"-d") == 0){
            //printk("Running DFS\n");
            DFS(task);
        }else if(strcmp(flag,"-b") == 0){
            //printk("Running BFS\n");
            BFS(task);
        }else{
            printk("invalid arg\n");
        }

        
    


    }
    

 return 0;
}


void __exit traverse_exit(void) {
 printk(KERN_INFO "Goodbye, World!\n");
}




module_init(traverse_init);
module_exit(traverse_exit);
