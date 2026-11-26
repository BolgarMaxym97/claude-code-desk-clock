package claude

import (
	"bytes"
	"encoding/json"
	"errors"
)

// object is a JSON object that keeps its key order (settings.json stays readable for the user).
type member struct {
	k string
	v json.RawMessage
}
type object []member

func parse(b []byte) (object, error) {
	if len(bytes.TrimSpace(b)) == 0 {
		return object{}, nil
	}
	d := json.NewDecoder(bytes.NewReader(b))
	t, err := d.Token()
	if err != nil {
		return nil, err
	}
	if t != json.Delim('{') {
		return nil, errors.New("not a JSON object")
	}
	var o object
	for d.More() {
		kt, err := d.Token()
		if err != nil {
			return nil, err
		}
		var v json.RawMessage
		if err := d.Decode(&v); err != nil {
			return nil, err
		}
		o = append(o, member{kt.(string), v})
	}
	if _, err := d.Token(); err != nil {
		return nil, err
	}
	return o, nil
}

func (o object) get(k string) json.RawMessage {
	for _, m := range o {
		if m.k == k {
			return m.v
		}
	}
	return nil
}

func (o *object) set(k string, v json.RawMessage) {
	for i := range *o {
		if (*o)[i].k == k {
			(*o)[i].v = v
			return
		}
	}
	*o = append(*o, member{k, v})
}

func (o *object) del(k string) {
	for i := range *o {
		if (*o)[i].k == k {
			*o = append((*o)[:i], (*o)[i+1:]...)
			return
		}
	}
}

func (o object) keys() []string {
	var ks []string
	for _, m := range o {
		ks = append(ks, m.k)
	}
	return ks
}

func (o object) encode() json.RawMessage {
	var b bytes.Buffer
	b.WriteByte('{')
	for i, m := range o {
		if i > 0 {
			b.WriteByte(',')
		}
		k, _ := json.Marshal(m.k)
		b.Write(k)
		b.WriteByte(':')
		b.Write(m.v)
	}
	b.WriteByte('}')
	return b.Bytes()
}

func (o object) pretty() []byte {
	var b bytes.Buffer
	json.Indent(&b, o.encode(), "", "  ")
	b.WriteByte('\n')
	return b.Bytes()
}
