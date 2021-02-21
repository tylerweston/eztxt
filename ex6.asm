	addi $sp,$sp, -4	# Adjust stack pointer
	sw $s0,0($sp)		# Save $s0 on the stack

	# Run the function
	add $s0,$a0,$a1		# Sum = sum + i

	# Save the return value in $v0
	move $v0,$s0

	# Restore overwritten registers from the stack
	lw $s0,0($sp)
	addi $sp,$sp,4		# Adjust stack pointer
	
	# Return from function
	jr $ra			# Jump to addr stored in $ra

	.data
msg1:	.asciiz	"Number of integers (N)?  "
msg2:	.asciiz	"Sum = "
lf:     .asciiz	"\n"