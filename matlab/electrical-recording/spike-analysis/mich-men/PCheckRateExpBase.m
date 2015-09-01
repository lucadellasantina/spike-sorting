function ok = PCheckRateExpBase(p)
ok = 1;
% No negative rate constants
if (~isempty(find(p(4:5,:) < 0)))
	ok = 0;
end
% Keep tdelay in range [0,2]
if (~isempty(find(p(1,:) > 2)) | ~isempty(find(p(1,:) < 0)))
	ok = 0;
end